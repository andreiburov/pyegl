#include <torch/extension.h>

#include <vector>
#include <cassert>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
//#include <type_traits>

#include "OpenGL_Helper.h"
#include "path.h"


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

static OpenGL::Mesh mesh;
static GLint position_loc, normal_loc, color_loc, uv_loc, mask_loc;
static OpenGL::Transformations transformations;
static std::vector<OpenGL::mat4> rigids;
static unsigned int frame_count = 0;
static unsigned int g_width = 512;
static unsigned int g_height = 512;


void pyegl_init(unsigned int width, unsigned int height)
{
  g_width = width;
  g_height = height;

  // init egl context
  eglContext.Init(width, height);

  // init shader program
  path so_path(so_path_lookup());
  if(!shaderProgram.Init((so_path.parent_path() / "shaders/vertexShader.glsl").str(), (so_path.parent_path() / "shaders/geometryShader.glsl").str(), (so_path.parent_path() / "shaders/fragmentShader.glsl").str()))
  {
      std::cout << "ERROR: initializing shader program failed" << std::endl;
      return;
  }

  // uniform location
  std::cout << "uniform location" << std::endl;
  shaderProgram.Use();
  transformations.SetUniformLocations(shaderProgram.GetUniformLocation("projection"), shaderProgram.GetUniformLocation("modelview"), shaderProgram.GetUniformLocation("mesh_normalization") );

  // attribute location
  std::cout << "attribute location" << std::endl;
  shaderProgram.Use();
  position_loc = shaderProgram.GetAttribLocation("in_position");
  if (position_loc < 0) return;
  normal_loc = shaderProgram.GetAttribLocation("in_normal");
  color_loc = shaderProgram.GetAttribLocation("in_color");
  uv_loc = shaderProgram.GetAttribLocation("in_uv");
  mask_loc = shaderProgram.GetAttribLocation("in_mask");
  
  // render target
  std::cout << "create rendertarget" << std::endl;
  renderTarget.Init(eglContext.GetWidth(), eglContext.GetHeight());

  internal_state = InternalState::INITIALIZED;
}


void pyegl_terminate()
{
  internal_state = InternalState::UNINITIALIZED;
  mesh.Terminate();
  renderTarget.Terminate();
  eglContext.Terminate();
}


void render(std::vector<float>& intrinsics)
{
  float fovX, fovY, cX, cY, near, far;

  if (intrinsics.size() != 6)
  {
    fovX = 4.14423; 
    fovY = 4.27728;
    cX = 0.5;
    cY = 0.5;
    near = 0.1;
    far = 10.0;
  }
  else
  {
    fovX = intrinsics[0];
    fovY = intrinsics[1];
    cX = intrinsics[2];
    cY = intrinsics[3];
    near = intrinsics[4];
    far = intrinsics[5];
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
  transformations.SetProjection(fovX, fovY, cX, cY, near, far);
  transformations.SetMeshNormalization(mesh.GetCoG(), mesh.GetExtend());
  transformations.Use();

  // render mesh
  mesh.Render(position_loc, normal_loc, color_loc, uv_loc, mask_loc);
  renderTarget.CopyRenderedTexturesToCUDA();

  //renderTarget.CopyRenderedTexturesToCUDA(true);
  //renderTarget.WriteDataToFile("results/cuda_color_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[0], 0);
  //renderTarget.WriteDataToFile("results/cuda_position_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[1], 1);
  //renderTarget.WriteDataToFile("results/cuda_normal_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[2], 2);
  //renderTarget.WriteDataToFile("results/cuda_uv_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[3], 3);
  //renderTarget.WriteDataToFile("results/cuda_bary_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[4], 4);
  //renderTarget.WriteDataToFile("results/cuda_vids_" + std::to_string(frame_count) + ".png", renderTarget.GetBuffers()[5], 5);

  // write rendertarget to file
  renderTarget.WriteToFile("results/fbo_color_" + std::to_string(frame_count) + ".png", 0);
  renderTarget.WriteToFile("results/fbo_position_" + std::to_string(frame_count) + ".png", 1);
  renderTarget.WriteToFile("results/fbo_normal_" + std::to_string(frame_count) + ".png", 2);
  renderTarget.WriteToFile("results/fbo_uv_" + std::to_string(frame_count) + ".png", 3);
  renderTarget.WriteToFile("results/fbo_bary_" + std::to_string(frame_count) + ".png", 4);
  renderTarget.WriteToFile("results/fbo_vids_" + std::to_string(frame_count) + ".png", 5);

  // save screenshot
  // eglContext.SaveScreenshotPPM("results/rendering_" + std::to_string(frame_count) + ".ppm");
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
  std::cout << "pyegl_forward" << std::endl;
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

  return {color_map, position_map, normal_map, uv_map, bary_map, vids_map};
}


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
  m.def("init", &pyegl_init, "Set up EGL context");
  m.def("terminate", &pyegl_terminate, "Destroy EGL context");
  m.def("forward", &pyegl_forward, "Forward through pyegl");
  m.def("so_path_lookup", &so_path_lookup, "Lookup where pyegl so is installed");
}
