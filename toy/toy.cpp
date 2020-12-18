#include <torch/extension.h>

#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>


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


std::vector<torch::Tensor> toy_forward(std::vector<float> intrinsics, torch::Tensor vertices, torch::Tensor faces)
{
  std::cout << "intrinsics: ";
  for (const auto& v: intrinsics)
  {
    std::cout << v << " ";
  }
  std::cout << std::endl;

  std::cout << "vertices: " << vertices.toString() << std::endl;
  std::cout << "faces: " << faces.toString() << std::endl;

  path so_path{so_path_lookup()};
  std::ifstream file((so_path.parent_path() / "shaders/vertexShader.glsl").str(), std::ios::in);
  if (!file)
  {
    std::cerr << "Can't open file shaders/vertexShader.glsl" << std::endl;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  std::cout << "Vertex Shader:" << std::endl;
  std::cout << buffer.str() << std::endl;

  return {vertices, faces};
}


PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
  m.def("forward", &toy_forward, "Forward through toy");
  m.def("so_path_lookup", &so_path_lookup, "Lookup where toy so is installed");
}
