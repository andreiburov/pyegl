#include <torch/extension.h>

#include <vector>
#include <cassert>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <chrono>

#include "opengl_helper.h"
#include "deps/path.h"


//#define DEBUG
#define CLOCK_START(start) auto start = std::chrono::system_clock::now()
#define CLOCK_END(start, msg) std::cout << msg << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count() << "ms" << std::endl

#ifndef DEBUG
#define CLOCK_START(start) {}
#define CLOCK_END(start, msg) {}
#endif


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>


const char* so_path_lookup() 
{
  Dl_info dl_info;
  dladdr((void*)so_path_lookup, &dl_info);
  return dl_info.dli_fname;
}


enum InternalState 
{
  UNINITIALIZED,
  INITIALIZED
};


static InternalState internal_state = InternalState::UNINITIALIZED;
static OpenGL::EGL eglContext;
static OpenGL::RenderTarget renderTarget;
static OpenGL::ShaderProgram shaderProgram;
static OpenGL::Texture texture;
static OpenGL::Mesh mesh;
static GLint position_loc, normal_loc, color_loc, uv_loc, mask_loc;
static OpenGL::Transformations transformations;
static std::vector<OpenGL::mat4> rigids;
static unsigned int frame_count = 0;
static unsigned int g_width = 512;
static unsigned int g_height = 512;


void pyegl_init_with_defines(unsigned int width, unsigned int height, std::vector<std::string> defines)
{
  g_width = width;
  g_height = height;

  // init egl context
  eglContext.Init(width, height);

  // init shader program
  path so_path(so_path_lookup());
  if(!shaderProgram.Init((so_path.parent_path() / "shaders/basic.vs").str(),
                         (so_path.parent_path() / "shaders/basic.gs").str(), 
                         (so_path.parent_path() / "shaders/basic.fs").str(),
                         defines))
  {
      std::cout << "ERROR: initializing shader program failed" << std::endl;
      return;
  }

  std::cout << "Attach transformations" << std::endl;
  std::cout << " " << "Uniform locations" << std::endl;
  shaderProgram.Use();
  transformations.SetUniformLocations(shaderProgram.GetUniformLocation("projection"), shaderProgram.GetUniformLocation("modelview"), shaderProgram.GetUniformLocation("mesh_normalization") );

  std::cout << " " << "Attribute locations" << std::endl;
  shaderProgram.Use();
  position_loc = shaderProgram.GetAttribLocation("in_position");
  normal_loc = shaderProgram.GetAttribLocation("in_normal");
  color_loc = shaderProgram.GetAttribLocation("in_color");
  uv_loc = shaderProgram.GetAttribLocation("in_uv");
  mask_loc = shaderProgram.GetAttribLocation("in_mask");
  
  std::cout << "Create rendertarget" << std::endl;
  renderTarget.Init(eglContext.GetWidth(), eglContext.GetHeight());

  internal_state = InternalState::INITIALIZED;
}


void pyegl_init(unsigned int width, unsigned int height)
{
  pyegl_init_with_defines(width, height, {"PHONG_SHADING"});
}


void pyegl_terminate()
{
  internal_state = InternalState::UNINITIALIZED;
  mesh.Terminate();
  texture.Terminate();
  renderTarget.Terminate();
  eglContext.Terminate();
}


void pyegl_attach_texture(std::string filename)
{
  std::cout << "Attach texture" << std::endl;
  texture.Init(filename.c_str());
  shaderProgram.Use();
  texture.SetUniformLocations(shaderProgram.GetUniformLocation("color_texture"));
}


void render(std::vector<float>& intrinsics)
{
  float fovX, fovY, cX, cY, near, far;

  if (intrinsics.size() == 6)
  {
    fovX = intrinsics[0];
    fovY = intrinsics[1];
    cX = intrinsics[2];
    cY = intrinsics[3];
    near = intrinsics[4];
    far = intrinsics[5];
  }
  else
  {
    std::cout << "ERROR: intrinsics have less then 6 components" << std::endl;
    return;
  }

  // reset viewport, clear
  eglContext.Clear();

  renderTarget.Use();
  renderTarget.Clear();

  // enable depth test
  glDepthRangef(near, far);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  // set shader program
  shaderProgram.Use();

  // set uniforms
  transformations.SetModelView(rigids[0]);
  transformations.SetPinholeProjection(fovX, fovY, cX, cY, near, far, g_width, g_height);

  //#ifdef DEBUG
  //std::cout << "Pinhole camera:" << std::endl;
  //std::cout << " " << transformations.projection.m00 << " " << transformations.projection.m01 << " " << transformations.projection.m02 << " " << transformations.projection.m03 << std::endl;
  //std::cout << " " << transformations.projection.m10 << " " << transformations.projection.m11 << " " << transformations.projection.m12 << " " << transformations.projection.m13 << std::endl;
  //std::cout << " " << transformations.projection.m20 << " " << transformations.projection.m21 << " " << transformations.projection.m22 << " " << transformations.projection.m23 << std::endl;
  //std::cout << " " << transformations.projection.m30 << " " << transformations.projection.m31 << " " << transformations.projection.m32 << " " << transformations.projection.m33 << std::endl;
  //#endif

  transformations.SetMeshNormalization(mesh.GetCoG(), mesh.GetExtend());

  #ifdef DEBUG
  std::cout << "Mesh normalization:" << std::endl;
  auto cog = mesh.GetCoG();
  std::cout << " " << cog(0) << " " << cog(1) << " " << cog(2) << std::endl;
  std::cout << " " << mesh.GetExtend() << std::endl;
  #endif

  transformations.Use();
  texture.Use();

  // render mesh
  CLOCK_START(time_render);
  mesh.Render(position_loc, normal_loc, color_loc, uv_loc, mask_loc);
  CLOCK_END(time_render, "Rendering: ");

  CLOCK_START(time_opengl_cuda_transfer);
  renderTarget.CopyRenderedTexturesToCUDA();
  CLOCK_END(time_opengl_cuda_transfer, "Copying OpenGL to CUDA: ");

  #ifdef DEBUG
  renderTarget.CopyRenderedTexturesToCUDA(true);
  //renderTarget.WriteDataToFile("results/cuda_color_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[0], 0);
  //renderTarget.WriteDataToFile("results/cuda_position_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[1], 1);
  //renderTarget.WriteDataToFile("results/cuda_normal_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[2], 2);
  //renderTarget.WriteDataToFile("results/cuda_uv_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[3], 3);
  //renderTarget.WriteDataToFile("results/cuda_bary_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[4], 4);
  //renderTarget.WriteDataToFile("results/cuda_vids_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[5], 5);

  // write rendertarget to file
  renderTarget.WriteToFile("fbo_color_" + std::to_string(frame_count) + ".png", 0);
  renderTarget.WriteToFile("fbo_position_" + std::to_string(frame_count) + ".png", 1);
  renderTarget.WriteToFile("fbo_normal_" + std::to_string(frame_count) + ".png", 2);
  renderTarget.WriteToFile("fbo_uv_" + std::to_string(frame_count) + ".png", 3);
  renderTarget.WriteToFile("fbo_bary_" + std::to_string(frame_count) + ".png", 4);
  renderTarget.WriteToFile("fbo_vids_" + std::to_string(frame_count) + ".png", 5);

  // save screenshot
  eglContext.SaveScreenshotPPM("rendering_" + std::to_string(frame_count) + ".ppm");
  #endif

  frame_count++;

  // flush and swap buffers
  eglContext.SwapBuffer();
}

std::vector<unsigned int> map_indices(const torch::Tensor& indices, unsigned int n_faces)
{
  std::vector<unsigned int> gl_indices;
  gl_indices.reserve(n_faces*3);
  for (unsigned int i = 0; i < n_faces*3; i++)
  {
    gl_indices.emplace_back(static_cast<unsigned int>(((long*)indices.data_ptr())[i]));
  }

  return gl_indices;
}

std::vector<torch::Tensor> pyegl_forward(std::vector<float> intrinsics, std::vector<float> pose, torch::Tensor vertices, unsigned int n_vertices, torch::Tensor indices, unsigned int n_faces)
{
  if (internal_state != InternalState::INITIALIZED)
  {
    std::cout << "ERROR: you need to initialize pyegl" << std::endl;
    return {};
  }

  if (vertices.scalar_type() != torch::kFloat32)
  {
    std::cout << "ERROR: vertices has to be float32, but was: " << vertices.scalar_type() << std::endl;
    return {};
  }

  if (!vertices.is_cuda())
  {
    std::cout << "WARNING: vertices should be placed on CUDA, but was: " << vertices.device() << std::endl;
    return {};
  }

  if (indices.scalar_type() != torch::kInt64)
  {
    std::cout << "ERROR: indices has to be int64, but was: " << indices.scalar_type() << std::endl;
    return {};
  }
  
  if (indices.device() != torch::kCPU)
  {
    std::cout << "ERROR: faces has to be placed on CPU, but was: " << indices.device() << std::endl;
    return {};
  }

  CLOCK_START(time_pytorch_opengl_transfer);
  if (!mesh.IsInitialized())
  {
    mesh.Init((OpenGL::Vertex*)vertices.data_ptr(), n_vertices, map_indices(indices, n_faces).data(), n_faces, vertices.is_cuda());
  }
  else if (mesh.GetNumberOfVertices() != n_vertices || mesh.GetNumberOfFaces() != n_faces || mesh.IsVertexDataOnCUDA() != vertices.is_cuda())
  {
    //https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawElements.xhtml
    //type must be on of GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT
    std::cout << "ERROR: Different amount of vertices or faces in subsequent call: (" << n_vertices << "|" << n_faces << ")" << std::endl;
    mesh.Terminate();
    mesh.Init((OpenGL::Vertex*)vertices.data_ptr(), n_vertices, map_indices(indices, n_faces).data(), n_faces, vertices.is_cuda());
  }
  else
  {
    mesh.Update((OpenGL::Vertex*)vertices.data_ptr(), n_vertices, vertices.is_cuda());
  }
  CLOCK_END(time_pytorch_opengl_transfer, "Copying Pytorch to OpenGL: ");
  
  OpenGL::mat4 m{};
  for (size_t i = 0; i < pose.size(); i++)
  {
    m.data[i] = pose[i];
  }
  Eigen::Matrix4f mEigen = m.ToEigen();
  mEigen = mEigen.inverse().eval();
  m.FromEigen(mEigen);
  rigids.push_back(m);

  render(intrinsics);

  CLOCK_START(time_cuda_pytorch_transfer);
  auto device = vertices.device();
  auto color_options = torch::TensorOptions().dtype(torch::kFloat32).layout(torch::kStrided).device(device);
  auto color_map = torch::from_blob(renderTarget.GetBuffers()[0], {g_height, g_width, 4}, color_options);
  auto position_options = torch::TensorOptions().dtype(torch::kFloat32).layout(torch::kStrided).device(device);
  auto position_map = torch::from_blob(renderTarget.GetBuffers()[1], {g_height, g_width, 4}, position_options);
  auto normal_options = torch::TensorOptions().dtype(torch::kFloat32).layout(torch::kStrided).device(device);
  auto normal_map = torch::from_blob(renderTarget.GetBuffers()[2], {g_height, g_width, 4}, normal_options);
  auto uv_options = torch::TensorOptions().dtype(torch::kFloat32).layout(torch::kStrided).device(device);
  auto uv_map = torch::from_blob(renderTarget.GetBuffers()[3], {g_height, g_width, 2}, uv_options);
  auto bary_options = torch::TensorOptions().dtype(torch::kFloat32).layout(torch::kStrided).device(device);
  auto bary_map = torch::from_blob(renderTarget.GetBuffers()[4], {g_height, g_width, 4}, bary_options);
  auto vids_options = torch::TensorOptions().dtype(torch::kFloat32).layout(torch::kStrided).device(device);
  auto vids_map = torch::from_blob(renderTarget.GetBuffers()[5], {g_height, g_width, 4}, vids_options);
  CLOCK_END(time_cuda_pytorch_transfer, "Copying CUDA to Pytorch: ");

  return {color_map, position_map, normal_map, uv_map, bary_map, vids_map};
}


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
  m.def("init", &pyegl_init, "Set up EGL context");
  m.def("init_with_defines", &pyegl_init_with_defines, "Set up EGL context with defines");
  m.def("terminate", &pyegl_terminate, "Destroy EGL context");
  m.def("attach_texture", &pyegl_attach_texture, "Load texture from file and attach to context");
  m.def("forward", &pyegl_forward, "Forward through pyegl");
  m.def("so_path_lookup", &so_path_lookup, "Lookup where pyegl so is installed");
}
