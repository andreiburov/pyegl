#include <torch/extension.h>

#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>


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


void pyegl_init(unsigned int width, unsigned int height)
{
  eglContext.Init(width, height);
  internal_state = InternalState::INITIALIZED;
}


void pyegl_terminate()
{
  internal_state = InternalState::UNINITIALIZED;
  eglContext.Terminate();
}


void render()
{
    float near = 0.1;
    float far = 10.0;
    float fovX = 4.14423;
    float fovY = 4.27728;
    float cX = 0.5;
    float cY = 0.5;

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
    renderTarget.WriteToFile("results/fbo_color_rendering_" + std::to_string(0) + ".png", 0);
    renderTarget.WriteToFile("results/fbo_position_rendering_" + std::to_string(0) + ".png", 1);
    renderTarget.WriteToFile("results/fbo_normal_rendering_" + std::to_string(0) + ".png", 2);
    renderTarget.WriteToFile("results/fbo_uv_rendering_" + std::to_string(0) + ".png", 3);
    renderTarget.WriteToFile("results/fbo_bary_rendering_" + std::to_string(0) + ".png", 4);
    renderTarget.WriteToFile("results/fbo_vids_rendering_" + std::to_string(0) + ".png", 5);

    // save screenshot
    eglContext.SaveScreenshotPPM("results/rendering_" + std::to_string(0) + ".ppm");

    // flush and swap buffers
    eglContext.SwapBuffer();
}


std::vector<torch::Tensor> pyegl_forward(std::vector<float> intrinsics, torch::Tensor vertices, torch::Tensor faces)
{
  std::cout << "pyegl_forward" << std::endl;
  if (internal_state != InternalState::INITIALIZED)
  {
    std::cout << "ERROR: you need to initialize pyegl" << std::endl;
    return {};
  }

  // init shader program
  path so_path(so_path_lookup());
  if(!shaderProgram.Init((so_path.parent_path() / "shaders/vertexShader.glsl").str(), (so_path.parent_path() / "shaders/geometryShader.glsl").str(), (so_path.parent_path() / "shaders/fragmentShader.glsl").str()))
  {
      std::cout << "ERROR: initializing shader program failed" << std::endl;
      return {};
  }

  // uniform location
  std::cout << "uniform location" << std::endl;
  shaderProgram.Use();
  transformations.SetUniformLocations(shaderProgram.GetUniformLocation("projection"), shaderProgram.GetUniformLocation("modelview"), shaderProgram.GetUniformLocation("mesh_normalization") );

  // attribute location
  std::cout << "attribute location" << std::endl;
  shaderProgram.Use();
  position_loc = shaderProgram.GetAttribLocation("in_position");
  if (position_loc < 0) return {};
  normal_loc = shaderProgram.GetAttribLocation("in_normal");
  color_loc = shaderProgram.GetAttribLocation("in_color");
  uv_loc = shaderProgram.GetAttribLocation("in_uv");
  mask_loc = shaderProgram.GetAttribLocation("in_mask");

  std::cout << "load mesh data" << std::endl;
  mesh.LoadObjFile("data/bunny_col.obj", 1.0f);


  std::cout << "load rigid transformations" << std::endl;
  {
      std::ifstream file("data/rigid.txt");
      if (!file.is_open())
      {
          rigids.push_back(OpenGL::mat4::Identity());
          std::cout << "WARNING: unable to load rigid transformations (using identity now)!" << std::endl;
      }
      else
      {
          while(file.good())
          {
              OpenGL::mat4 m{};
              file  >> m.m00 >> m.m01 >> m.m02 >> m.m03
                    >> m.m10 >> m.m11 >> m.m12 >> m.m13
                    >> m.m20 >> m.m21 >> m.m22 >> m.m23
                    >> m.m30 >> m.m31 >> m.m32 >> m.m33;

              Eigen::Matrix4f mEigen = m.ToEigen();
              mEigen = mEigen.inverse().eval();
              m.FromEigen(mEigen);
              rigids.push_back(m);
          }
          file.close();
      }

  }

  std::cout << "create rendertarget" << std::endl;
  renderTarget.Init(eglContext.GetWidth(), eglContext.GetHeight());

  render();

  return {vertices, faces};
}


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
  m.def("init", &pyegl_init, "Set up EGL context");
  m.def("terminate", &pyegl_terminate, "Destroy EGL context");
  m.def("forward", &pyegl_forward, "Forward through pyegl");
  m.def("so_path_lookup", &so_path_lookup, "Lookup where pyegl so is installed");
}
