#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include <fstream>
#include <streambuf>

#include "OpenGL_Helper.h"
#include <cuda_gl_interop.h>
#include "helper_cuda.h"


using namespace std;


OpenGL::EGL eglContext;

float near = 0.1;
float far = 10.0;

float fovX = 4.14423;
float fovY = 4.27728;
float cX = 0.5;
float cY = 0.5;

unsigned int width = 512;
unsigned int height = 512;


OpenGL::Mesh mesh;

OpenGL::RenderTarget renderTarget;

OpenGL::ShaderProgram shaderProgram;
GLint position_loc, normal_loc, color_loc, uv_loc, mask_loc; // attribute location

std::vector<OpenGL::mat4> rigids;

OpenGL::Transformations transformations;


void Render()
{
    static int n_frames = 1;
    static int frameCnt = 0;
    if (frameCnt == n_frames) exit(0);
    OpenGL::ProgressBar("Rendering", float(frameCnt) / float(n_frames));

    // reset viewport, clear
    eglContext.Clear();

    renderTarget.Use();
    renderTarget.Clear();

    // enable depth test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // set shader program
    shaderProgram.Use();

    // set uniforms
    transformations.SetModelView(rigids[frameCnt % rigids.size()]);
    transformations.SetProjection(fovX, fovY, cX, cY, near, far);
    transformations.SetMeshNormalization(mesh.GetCoG(), mesh.GetExtend());
    transformations.Use();

    // render mesh
    mesh.Render(position_loc, normal_loc, color_loc, uv_loc, mask_loc);

    // pass data from OpenGL to CUDA
    checkCudaErrors(cudaGraphicsMapResources(renderTarget.GetNumOfGraphicsResources(), renderTarget.GetDestGraphicsResources()));

    //https://stackoverflow.com/questions/9406844/cudagraphicsresourcegetmappedpointer-returns-unknown-error
    //checkCudaErrors(cudaGraphicsResourceGetMappedPointer((void **)&cuda_buffer, &cuda_buffer_size, renderTarget.GetDestGraphicsResources()[0]));

    // special opaque structure for storing textures
    cudaArray* cuda_array;
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(&cuda_array, renderTarget.GetDestGraphicsResources()[0], 0, 0));

    //checkCudaErrors(cudaMemcpy2DFromArray(renderTarget.GetCudaResources()[0], 4 * sizeof(float) * width, cuda_array, 0, 0, 4 * sizeof(float) * width, height, cudaMemcpyDeviceToDevice));
    checkCudaErrors(cudaMemcpy2DFromArray(renderTarget.GetHostResources()[0], 4 * sizeof(float) * width, cuda_array, 0, 0, 4 * sizeof(float) * width, height, cudaMemcpyDeviceToHost));

    checkCudaErrors(cudaGraphicsUnmapResources(renderTarget.GetNumOfGraphicsResources(), renderTarget.GetDestGraphicsResources()));

    renderTarget.WriteDataToFile("../results/cuda_color_" + std::to_string(frameCnt) + ".png", renderTarget.GetHostResources()[0], 4 * sizeof(float) * width * height);

    // write rendertarget to file
    renderTarget.WriteToFile("../results/fbo_color_rendering_" + std::to_string(frameCnt) + ".png", 0);
    renderTarget.WriteToFile("../results/fbo_position_rendering_" + std::to_string(frameCnt) + ".png", 1);
    renderTarget.WriteToFile("../results/fbo_normal_rendering_" + std::to_string(frameCnt) + ".png", 2);
    renderTarget.WriteToFile("../results/fbo_uv_rendering_" + std::to_string(frameCnt) + ".png", 3);
    renderTarget.WriteToFile("../results/fbo_bary_rendering_" + std::to_string(frameCnt) + ".png", 4);
    renderTarget.WriteToFile("../results/fbo_vids_rendering_" + std::to_string(frameCnt) + ".png", 5);

    // save screenshot
    eglContext.SaveScreenshotPPM("../results/rendering_" + std::to_string(frameCnt) + ".ppm");

    // flush and swap buffers
    eglContext.SwapBuffer();

    frameCnt++;
}


static int opengl()  {

    std::cout << "OpenGL" << std::endl;

    // set depth range
    glDepthRangef(near, far);

    // init shader program
    if(!shaderProgram.Init("../shaders/vertexShader.glsl", "../shaders/geometryShader.glsl", "../shaders/fragmentShader.glsl"))
        //if(!shaderProgram.Init("vertexShader.glsl", "fragmentShader.glsl"))
    {
        std::cout << "ERROR: initializing shader program failed" << std::endl;
        return -1;
    }

    // uniform location
    std::cout << "uniform location" << std::endl;
    shaderProgram.Use();
    //transformations.SetUniformLocations(shaderProgram.GetUniformLocation("projection"), shaderProgram.GetUniformLocation("modelview"), shaderProgram.GetUniformLocation("mesh_normalization"));
    transformations.SetUniformLocations(shaderProgram.GetUniformLocation("projection"), shaderProgram.GetUniformLocation("modelview"), shaderProgram.GetUniformLocation("mesh_normalization") );

    // attribute location
    std::cout << "attribute location" << std::endl;
    shaderProgram.Use();
    position_loc = shaderProgram.GetAttribLocation("in_position");
    if (position_loc < 0) return -1;
    normal_loc = shaderProgram.GetAttribLocation("in_normal");
    color_loc = shaderProgram.GetAttribLocation("in_color");
    uv_loc = shaderProgram.GetAttribLocation("in_uv");
    mask_loc = shaderProgram.GetAttribLocation("in_mask");

    std::cout << "load mesh data" << std::endl;
    //mesh.Init(pvertex, 3, pindices, 1);
    //mesh.LoadOffFile("../data/simple.off", 1.0f);
    //mesh.LoadObjFile("../data/bunny.obj", 1.0f);
    mesh.LoadObjFile("../data/bunny_col.obj", 1.0f);


    std::cout << "load rigid transformations" << std::endl;
    {
        std::ifstream file("../data/rigid.txt");
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

    std::cout << "start render loop" << std::endl;
    while (true)
    {
        Render();
    }

    return 1;
}


void atexit_handler()
{
    renderTarget.Terminate();
    std::cout << "Terminate allocated resources" << std::endl;
}


int main(int argc, char *argv[])
{
    std::atexit(atexit_handler);
    eglContext.Init(width, height);
    opengl();
    eglContext.Terminate();
    return 0;
}
