#ifndef OPENGL_HELPER_H
#define OPENGL_HELPER_H

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <cmath>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <vector>
#include <exception>

#include "eigen/Eigen/Eigen"

////////////////////////////////
////   OPENGL / EGL / GLEW  ////
////////////////////////////////
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glew.h>

////////////////////////////////
///////       CUDA      ////////
////////////////////////////////
#include <cuda_gl_interop.h>
#include "cuda_helper.h"

////////////////////////////////
//////////   MACROS  ///////////
////////////////////////////////

#define V_RETURN(x) { if(x<0) {std::cout << std::endl << "[" << __FUNCTION__ << "] [" << __LINE__ << "] FAILED!" << std::endl; return -1;} }

#ifndef SAFE_DELETE
#define SAFE_DELETE(ptr) {if(ptr!=nullptr) {delete ptr; ptr = nullptr;}}
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(ptr) {if(ptr!=nullptr) {delete[] ptr; ptr = nullptr;}}
#endif

#ifndef MINF
#define MINF -std::numeric_limits<float>::infinity()
#endif

#ifndef M_PI
#define M_PI 3.14159265359
#endif

#define BUFFER_OFFSET(i) ((void*)(i))

class EGLException: public std::exception
{
public:
    const char* message;
    EGLException(const char* mmessage): message(mmessage) {}

    virtual const char* what() const throw()
    {
        return this->message;
    }
};

class EGLReturnException: private EGLException
{
    using EGLException::EGLException;
};

class EGLErrorException: private EGLException
{
    using EGLException::EGLException;
};

#define checkEglError(message){ \
    EGLint err = eglGetError(); \
    if (err != EGL_SUCCESS) \
    { \
        std::cerr << "EGL Error " << std::hex << err << std::dec << " on line " <<  __LINE__ << std::endl; \
        throw EGLErrorException(message); \
    } \
}

#define checkEglReturn(x, message){ \
    if (x != EGL_TRUE) \
    { \
        std::cerr << "EGL returned not true on line " << __LINE__ << std::endl; \
        throw EGLReturnException(message); \
    } \
}

namespace OpenGL
{

void CheckError();

void eglPrintError(std::string const& context);

void eglPrintErrorAndExit(std::string const& context);

void ProgressBar(std::string titel, float progress);

class EGL
{
public:
    EGL()
    {
    }

    EGLDisplay GetEGLDisplayFromNative(NativeDisplayType native_display=EGL_DEFAULT_DISPLAY);

    int Init(unsigned int width=512, unsigned int height=512);

    void Terminate()
    {
        // Terminate EGL when finished
        std::cout << "Terminate EGL" << std::endl;
        eglTerminate(egl_display);
    }

    void Clear()
    {
        glViewport(0 , 0 , GetWidth() , GetHeight());
        glClearColor(0.0 , 0.0 , 0.0 , 1.);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void SwapBuffer()
    {
        glFlush();
        eglSwapBuffers(egl_display, egl_surface);  // get the rendered buffer to the screen
    }

    void SaveScreenshotPPM(const std::string& filename);

    unsigned int GetWidth()
    {
        return (unsigned int) pbufferWidth;
    }

    unsigned int GetHeight()
    {
        return (unsigned int) pbufferHeight;
    }

private:
    // EGL
    EGLDisplay  egl_display;
    EGLContext  egl_context;
    EGLSurface  egl_surface;

    // window settings
    int pbufferWidth;
    int pbufferHeight;
};


class Texture
{
public:
    enum InternalState 
    {
      UNINITIALIZED,
      INITIALIZED
    };

    void Init(const char* filename);

    void Terminate()
    {
        if (state == InternalState::INITIALIZED)
        {
            state = InternalState::UNINITIALIZED;
            glBindTexture(GL_TEXTURE_2D, texture);
            glDeleteTextures(1, &texture);
        }
    }

    void SetUniformLocations(GLint _texture_loc)
    {
        texture_loc = _texture_loc;
    }

    void Use()
    {
        if (state == InternalState::INITIALIZED)
        {
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(texture_loc, 0);
        }
    }

private:
    unsigned int texture;
    GLint texture_loc;
    InternalState state = InternalState::UNINITIALIZED;
};


class RenderTarget
{
public:
    int Init(unsigned int _width, unsigned int _height);

    void Terminate()
    {
        for (int i = 0; i < NUM_GRAPHICS_RESOURCES; i++)
        {
            cudaGraphicsUnregisterResource(graphics_resource[i]);
            cudaFree(buffer[i]);
        }
    }


    void Use()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height); 
    }

    void Clear()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(-1.0f, -1.0f, -1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

    void CopyRenderedTexturesToCUDA(bool copy_to_host=false);

    void WriteDataToFile(const std::string& filename, float* data, unsigned int tex_id=0);

    void WriteToFile(const std::string& filename, unsigned int tex_id=0, bool yFlip=true);

    int GetNumOfGraphicsResources()
    {
        return NUM_GRAPHICS_RESOURCES;
    }

    float** GetBuffers()
    {
        return buffer;
    }

private:
    unsigned int width, height;

    // color frame buffer object
    GLuint fbo = 0;

    // textures
    GLuint color_texture;
    GLuint position_texture;
    GLuint normal_texture;
    GLuint uv_texture;
    GLuint bary_texture;
    GLuint vids_texture;

    // cuda graphics resources
    static const int NUM_GRAPHICS_RESOURCES = 6;
    cudaGraphicsResource_t graphics_resource[NUM_GRAPHICS_RESOURCES];
    float* buffer[NUM_GRAPHICS_RESOURCES];

    // depth buffer
    GLuint depth_buffer;
};


union mat4
{
    float data[4*4];
    struct { // m_row_col
        float   m00, m01, m02, m03,
                m10, m11, m12, m13,
                m20, m21, m22, m23,
                m30, m31, m32, m33;
    };

    static OpenGL::mat4 Identity()
    {
        mat4 id = { 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f};
        return id;
    }

    Eigen::Matrix4f ToEigen()
    {
        Eigen::Matrix4f m;
        m <<    m00, m01, m02, m03,
                m10, m11, m12, m13,
                m20, m21, m22, m23,
                m30, m31, m32, m33;
        return m;
    }

    void FromEigen(Eigen::Matrix4f& m)
    {
        for(unsigned int j=0; j<4; ++j)
            for(unsigned int i=0; i<4; ++i)
                data[j*4 + i] = m(j,i);
    }

};

union vec4
{
    float data[4];
    struct {
        float   x,y,z,w;
    };

    vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

    static OpenGL::vec4 Zero()
    {
        vec4 id = { 0.0f, 0.0f, 0.0f, 0.0f};
        return id;
    }

    Eigen::Vector4f ToEigen()
    {
        Eigen::Vector4f v;
        v << x,y,z,w;
        return v;
    }

    void FromEigen(Eigen::Vector4f& v)
    {
        x = v.x();
        y = v.y();
        z = v.z();
        w = v.w();
    }

};

class Shader
{
public:
    int LoadShader(const char  *shader_source, GLenum type);

    int LoadShaderFromFile(const std::string& filename, GLenum type, const std::vector<std::string>& defines);

    GLuint GetID()
    {
        return shader;
    }


private:
    int print_shader_info_log();

    // shader
    GLuint  shader;
};

class ShaderProgram
{
public:
    int Init(Shader& vertexShader, Shader& fragmentShader);

    int Init(Shader& vertexShader, Shader& geometryShader, Shader& fragmentShader);
    
    int Init(Shader& computeShader);

    int Init(const std::string& filename_vertexShader, const std::string& filename_fragmentShader, const std::vector<std::string>& defines);

    int Init(const std::string& filename_vertexShader, const std::string& filename_geometryShader, const std::string& filename_fragmentShader, const std::vector<std::string>& defines);

    int Init(const std::string& filename_computeShader, const std::vector<std::string>& defines);

    void Use()
    {
        glUseProgram(shaderProgram);
    }

    GLint GetUniformLocation(const std::string& name)
    {
        GLint loc = glGetUniformLocation(shaderProgram, name.c_str());
        if (loc < 0)
        {
            std::cerr << " " << "Unable to get uniform location: " << name << "\t";
            std::cerr << "(Maybe unused in shader program?)" << std::endl;
        }
        return loc;
    }

    int SetUniform3fv(const std::string& name, const Eigen::Vector3f& value)
    {
        auto loc = GetUniformLocation(name);
        glUniform3fv(loc, 1, value.data());
        if (glGetError() != GL_NO_ERROR)
        {
            std::cout << "ERROR: setting a uniform " << name << std::endl;
            return -1;
        }

        return 1;
    }

    GLint GetAttribLocation(const std::string& name)
    {
        GLint loc = glGetAttribLocation(shaderProgram, name.c_str());
        if (loc < 0)
        {
            std::cerr << " " << "Unable to get attribute location: " << name << "\t";
            std::cerr << "(Maybe unused in shader program?)" << std::endl;
        }
        return loc;
    }

private:
    GLuint shaderProgram;
};


// shader transformations cpu -> gpu example
struct Transformation
{
    OpenGL::mat4 projection;
    OpenGL::mat4 modelview;
    OpenGL::vec4 mesh_normalization;
  
    // uniform location
    GLint projection_loc;
    GLint modelview_loc;
    GLint mesh_normalization_loc;
  
    Transformation()
    {
        Reset();
        projection_loc = modelview_loc = mesh_normalization_loc = -1;
    }
  
    void Reset()
    {
        projection = OpenGL::mat4::Identity();
        modelview = OpenGL::mat4::Identity();
        mesh_normalization = OpenGL::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
  
    void SetUniformLocations(GLint _projection_loc, GLint _modelview_loc, GLint _mesh_normalization_loc)
    {
        projection_loc = _projection_loc;
        modelview_loc = _modelview_loc;
        mesh_normalization_loc = _mesh_normalization_loc;
    }
  
    void Use()
    {
        glUniformMatrix4fv(projection_loc, 1, true, projection.data);
        glUniformMatrix4fv(modelview_loc, 1, true, modelview.data);
        glUniform4fv(mesh_normalization_loc, 1, mesh_normalization.data);
    }
  
    void SetModelView(OpenGL::mat4& m)
    {
        modelview = m;
    }
  
    void SetPerspectiveProjection(float fovX, float fovY, float cX, float cY, float near, float far);

    void SetWeakPerspectiveProjection(float fx, float fy, float cx, float cy);
  
    // https://github.com/mmatl/pyrender/blob/4a289a6205c5baa623cd0e7da1be3d898bcbc4da/pyrender/camera.py#L396
    void SetPinholeProjection(float fx, float fy, float cx, float cy, float near, float far, float width, float height);
  
    void SetPinholeZeroOpticalCenterProjection(float fx, float fy, float cx, float cy, float near, float far, float width, float height);

    void SetIdentityProjection();
  
    void SetMeshNormalization(Eigen::Vector3f cog, float scale)
    {
        mesh_normalization = {cog.x(), cog.y(), cog.z(), scale};
    }
  
    float FovX() { return projection.m00; }
    float FovY() { return projection.m11; }
    float CenterX() { return projection.m02; }
    float CenterY() { return projection.m12; }
  
    Eigen::Matrix4f MeshNormalization() {
        float scale = mesh_normalization.w;
        Eigen::Vector3f cog(mesh_normalization.x, mesh_normalization.y, mesh_normalization.z);
        Eigen::Matrix4f denormalization;
        denormalization <<  scale, 0.0f, 0.0f, cog.x(), 
                            0.0f, scale, 0.0f, cog.y(), 
                            0.0f, 0.0f, scale, cog.z(), 
                            0.0f, 0.0f, 0.0f, 1.0f;
        Eigen::Matrix4f normalization = denormalization.inverse();
        return normalization;
    }
  
    Eigen::Matrix4f World2CameraSpace()
    {
        Eigen::Matrix4f camM = modelview.ToEigen(); // maps vertices (worldspace) to camera space
        return camM;
    }
  
    Eigen::Matrix4f Camera2WorldSpace()
    {
        Eigen::Matrix4f camM_Inv = World2CameraSpace().inverse(); // maps from camera space to world space
        return camM_Inv;
    }
};

struct Vertex
{
  float x, y, z;        // Vertex
  float nx, ny, nz;     // Normal
  float r, g, b, a;       // color
  float u, v;           // Texcoord
  float mask;           // mask

  Vertex()
  : x(0), y(0), z(0), nx(0), ny(0), nz(1), r(1), g(1), b(0), a(1), u(0), v(0), mask(1)
  {
  }

  bool operator==(Vertex const& o) const {
      if (x != o.x || y != o.y || z != o.z)
          return false;
      if (u != o.u || v != o.v)
          return false;
      return true;
  }
};

template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

struct VertexHash {
    size_t operator()(OpenGL::Vertex const& v) const {
        std::size_t hash = std::hash<float>()(v.x);
        hash_combine(hash, v.y);
        hash_combine(hash, v.z);
        hash_combine(hash, v.u);
        hash_combine(hash, v.v);
        return hash;
    }
};


struct Mesh
{
public:
    Mesh(): verbose(false), initialized(false)
    {
    }

    int LoadObjFile(const std::string& filename, float scale=1.0f);

    int LoadOffFile(const std::string& filename, float scale=1.0f);

    void Terminate();

    void Init(OpenGL::Vertex* vertex_data, unsigned int n_vertices, unsigned int* indices, unsigned int n_faces, bool vertex_data_on_cuda=false);

    void Update(OpenGL::Vertex* vertex_data, unsigned int n_vertices, bool vertex_data_on_cuda=false);

    int Render(GLint position_loc, GLint normal_loc, GLint color_loc, GLint uv_loc, GLint mask_loc);

    GLuint GetVertexBufferID()
    {
        return VertexVBOID;
    }

    Eigen::Vector3f GetCoG()
    {
        return cog;
    }

    float GetExtend()
    {
        return extend;
    }

    const unsigned int GetNumberOfVertices() const
    {
        return n_vertices;
    }

    const unsigned int GetNumberOfFaces() const
    {
        return n_faces;
    }

    const bool IsInitialized() const
    {
        return initialized;
    }        
    
    const bool IsVertexDataOnCUDA() const
    {
        return vertex_data_on_cuda;
    }

private:
    GLuint vao;
    GLuint VertexVBOID, IndexVBOID;

    cudaGraphicsResource_t VertexVBORes;

    unsigned int n_vertices;
    unsigned int n_faces;

    bool vertex_data_on_cuda;
    bool verbose;
    bool initialized;

    Eigen::Vector3f cog; // center of gravity
    float extend; // extend of the mesh around center of gravity (bounding sphere)
};

}; // namespace OpenGL

// Eigen helper
template<typename T,unsigned int n,unsigned m>
std::istream &operator>>(std::istream &in, Eigen::Matrix<T,n,m> &other)
{
	for(unsigned int i=0; i<other.rows(); i++)
		for(unsigned int j=0; j<other.cols(); j++)
			in >> other(i,j);
	return in;
}

template<typename T,unsigned int n,unsigned m>
std::ostream &operator<<(std::ostream &out, const Eigen::Matrix<T,n,m> &other)
{
	std::fixed(out);
	for(int i=0; i<other.rows(); i++) {
		out << other(i,0);
		for(int j=1; j<other.cols(); j++) {
			out << " " << other(i,j);
		}
		out << std::endl;
	}
	return out;
}


#endif
