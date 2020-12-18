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


void pyegl_init(unsigned int width, unsigned int height)
{
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

  // write rendertarget to file
  renderTarget.WriteToFile("results/fbo_color_rendering_" + std::to_string(frame_count) + ".png", 0);
  renderTarget.WriteToFile("results/fbo_position_rendering_" + std::to_string(frame_count) + ".png", 1);
  renderTarget.WriteToFile("results/fbo_normal_rendering_" + std::to_string(frame_count) + ".png", 2);
  renderTarget.WriteToFile("results/fbo_uv_rendering_" + std::to_string(frame_count) + ".png", 3);
  renderTarget.WriteToFile("results/fbo_bary_rendering_" + std::to_string(frame_count) + ".png", 4);
  renderTarget.WriteToFile("results/fbo_vids_rendering_" + std::to_string(frame_count) + ".png", 5);

  // save screenshot
  eglContext.SaveScreenshotPPM("results/rendering_" + std::to_string(frame_count) + ".ppm");
  frame_count++;

  // flush and swap buffers
  eglContext.SwapBuffer();
}


std::vector<torch::Tensor> pyegl_forward(std::vector<float> intrinsics, std::vector<float> pose, torch::Tensor vertices, unsigned int n_vertices, torch::Tensor faces, unsigned int n_faces)
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

  if (faces.scalar_type() != torch::kInt64)
  {
    std::cout << "ERROR: faces has to be int64, but was: " << faces.scalar_type() << std::endl;
    return {};
  }

  std::vector<OpenGL::Vertex> pyegl_vertices;
  pyegl_vertices.reserve(10000);
  for (unsigned int i = 0; i < n_vertices; i++)
  {
    OpenGL::Vertex v;
    v.x = ((float*)vertices.data_ptr())[i*9 + 0];
    v.y = ((float*)vertices.data_ptr())[i*9 + 1];
    v.z = ((float*)vertices.data_ptr())[i*9 + 2];
    v.r = ((float*)vertices.data_ptr())[i*9 + 3];
    v.g = ((float*)vertices.data_ptr())[i*9 + 4];
    v.b = ((float*)vertices.data_ptr())[i*9 + 5];
    v.a = ((float*)vertices.data_ptr())[i*9 + 6];
    v.u = ((float*)vertices.data_ptr())[i*9 + 7];
    v.v = ((float*)vertices.data_ptr())[i*9 + 8];
    pyegl_vertices.emplace_back(v);   
  }

  std::vector<unsigned int> pyegl_indices;
  pyegl_indices.reserve(10000);
  for (unsigned int i = 0; i < n_faces*3; i++)
  {
    pyegl_indices.emplace_back(static_cast<unsigned int>(((long*)faces.data_ptr())[i]));
  }

  mesh.Update(pyegl_vertices.data(), n_vertices, pyegl_indices.data(), n_faces);

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

  return {vertices, faces};
}


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
  m.def("init", &pyegl_init, "Set up EGL context");
  m.def("terminate", &pyegl_terminate, "Destroy EGL context");
  m.def("forward", &pyegl_forward, "Forward through pyegl");
  m.def("so_path_lookup", &so_path_lookup, "Lookup where pyegl so is installed");
}
