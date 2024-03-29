#include "opengl_helper.h"

#ifndef NO_FREEIMAGE
#include "deps/FreeImageHelper.h"
#endif

#define TINYOBJLOADER_IMPLEMENTATION
#include "deps/tiny_obj_loader.h"
#include <unordered_map>
#include <functional>

#define STB_IMAGE_IMPLEMENTATION
#include "deps/stb_image.h"

namespace OpenGL
{

// Functions

void CheckError()
{
    int err = glGetError();

    switch(err)
    {
        //case GL_NO_ERROR: std::cout << "No error has been recorded. The value of this symbolic constant is guaranteed to be 0." << std::endl; break;
        case GL_INVALID_ENUM: std::cout << "An unacceptable value is specified for an enumerated argument. The offending command is ignored and has no other side effect than to set the error flag." << std::endl; break;
        case GL_INVALID_VALUE: std::cout << "A numeric argument is out of range. The offending command is ignored and has no other side effect than to set the error flag." << std::endl; break;
        case GL_INVALID_OPERATION: std::cout << "The specified operation is not allowed in the current state. The offending command is ignored and has no other side effect than to set the error flag." << std::endl; break;
        //case GL_INVALID_FRAMEBUFFER_OPERATION: std::cout << "The command is trying to render to or read from the framebuffer while the currently bound framebuffer is not framebuffer complete (i.e. the return value from glCheckFramebufferStatus is not GL_FRAMEBUFFER_COMPLETE). The offending command is ignored and has no other side effect than to set the error flag." << std::endl; break;
        case GL_OUT_OF_MEMORY: std::cout << "There is not enough memory left to execute the command. The state of the GL is undefined, except for the state of the error flags, after this error is recorded." << std::endl; break;
    }
}

void eglPrintError(std::string const& context)
{
    const GLint error=eglGetError();
    std::cerr << context << ": error 0x" << std::hex << int(error) << "\n";
}

void eglPrintErrorAndExit(std::string const& context)
{
    const GLint error=eglGetError();
    std::cerr << context << ": error 0x" << std::hex << int(error) << "\n";
    exit(1);
}

void ProgressBar(std::string titel, float progress)
{
    int barWidth = 70;

    std::cout << titel << " [";
    int pos = int(barWidth * progress);
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << " %\r";
    std::cout.flush();

    if (progress == 1.0f)	std::cout << std::endl;
}

// EGL

EGLDisplay EGL::GetEGLDisplayFromNative(NativeDisplayType native_display)
{
    std::cout << "Falling back to default display" << std::endl;
    EGLDisplay eglDisplay = eglGetDisplay(native_display);
    checkEglError("Failed to Get Display: eglGetDisplay");
    std::cerr << "Failback to eglGetDisplay" << std::endl;
    return eglDisplay;
}

int EGL::Init(unsigned int width, unsigned int height)
{        
    pbufferWidth = (int)width;
    pbufferHeight = (int)height;

    EGLint pbufferAttribs[] = {
        EGL_WIDTH, pbufferWidth,
        EGL_HEIGHT, pbufferHeight,
        EGL_NONE,
    };

    // https://gist.github.com/andyneff/36293b1aeb509fd1c6313afabac777ee
    // 1. Initialize EGL
    {
        std::cout << "EGL_DEVICE_ID environment variable is set to: " << std::getenv("EGL_DEVICE_ID") << std::endl;
        EGLint device_id = std::atoi(std::getenv("EGL_DEVICE_ID"));

        PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC) eglGetProcAddress("eglQueryDevicesEXT");
        checkEglError("Failed to get EGLEXT: eglQueryDevicesEXT");
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
        checkEglError("Failed to get EGLEXT: eglGetPlatformDisplayEXT");
        //PFNEGLQUERYDEVICEATTRIBEXTPROC eglQueryDeviceAttribEXT = (PFNEGLQUERYDEVICEATTRIBEXTPROC) eglGetProcAddress("eglQueryDeviceAttribEXT");
        //checkEglError("Failed to get EGLEXT: eglQueryDeviceAttribEXT");

        if (device_id >= 0)
        {
            EGLDeviceEXT *eglDevs;
            EGLint numberDevices;

            //Get number of devices
            checkEglReturn(eglQueryDevicesEXT(0, NULL, &numberDevices), "Failed to get number of devices. Bad parameter suspected");
            checkEglError("Error getting number of devices: eglQueryDevicesEXT");
            std::cerr << numberDevices << " EGL devices found." << std::endl;

            if (numberDevices > 0)
            {
                eglDevs = new EGLDeviceEXT[numberDevices];
                checkEglReturn(eglQueryDevicesEXT(numberDevices, eglDevs, &numberDevices), "Failed to get devices. Bad parameter suspected");
                checkEglError("Error getting number of devices: eglQueryDevicesEXT");

                egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, eglDevs[device_id], 0);
                checkEglError("Error getting Platform Display: eglGetPlatformDisplayEXT");
            }
            else
            {
                egl_display = GetEGLDisplayFromNative();
            }
        }
        else
        {
            egl_display = GetEGLDisplayFromNative();
        }

        if (egl_display == EGL_NO_DISPLAY) std::cout << "NO EGL DISPLAY" << std::endl;
        std::cout << "Initialize EGL" << std::endl;
    }

    EGLint egl_major_ver, egl_minor_ver;
    // https://github.com/demotomohiro/Reflection-Refraction-less-Ronpa-Raytracing-Renderer/blob/master/src/glcontext_egl.cpp
    if(eglInitialize(egl_display, &egl_major_ver, &egl_minor_ver) == EGL_FALSE)
    {
        std::cerr << "Failed to eglInitialize" << std::endl;
        EGLint error =  eglGetError();
        switch(error)
        {
        case EGL_BAD_DISPLAY:
            std::cerr << "display is not an EGL display connection" << std::endl;
            break;
        case EGL_NOT_INITIALIZED:
            std::cerr << "display cannot be initialized" << std::endl;
            break;
        case EGL_BAD_ACCESS:
            std::cerr << "EGL cannot access requested resource (display)" << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << error << std::endl;
            break;
        }
        return 0;
    }
    std::cout << "EGL version: " << egl_major_ver << "." << egl_minor_ver << std::endl;

    char const * client_apis = eglQueryString(egl_display, EGL_CLIENT_APIS);
    if(!client_apis)
    {
        std::cerr << "Failed to eglQueryString(egl_display, EGL_CLIENT_APIS)" << std::endl;
        return 0;
    }
    std::cout << "Supported client rendering APIs: " << client_apis << std::endl;

    // 2. Select an appropriate configuration
    EGLint numConfigs;
    EGLConfig eglCfg;
    static const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    }; 
    eglChooseConfig(egl_display, configAttribs, &eglCfg, 1, &numConfigs);

    // 3. Bind the API
    if(!eglBindAPI(EGL_OPENGL_API))
    {
        std::cout << "ERROR: unable to bind opengl API" << std::endl;
        return 0;
    }


    // 4. Create a surface
    egl_surface = eglCreatePbufferSurface(egl_display, eglCfg, pbufferAttribs);

    // 5. Create a context and make it current
    GLint gl_req_major_ver = 4;
    GLint gl_req_minor_ver = 6;
    
    //https://github.com/demotomohiro/Reflection-Refraction-less-Ronpa-Raytracing-Renderer/blob/master/src/glcontext_egl.cpp
    const EGLint context_attrib[] =
    {
        EGL_CONTEXT_MAJOR_VERSION_KHR,  gl_req_major_ver,
        EGL_CONTEXT_MINOR_VERSION_KHR,  gl_req_minor_ver,
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_FLAGS_KHR,          EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR
#ifndef NDEBUG
        | EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR
#endif
        ,
        EGL_NONE
    };
    egl_context = eglCreateContext(egl_display, eglCfg, EGL_NO_CONTEXT, context_attrib);

    if(egl_context == EGL_NO_CONTEXT)
    {
        std::cerr << "Failed to eglCreateContext" << std::endl;
        EGLint error =  eglGetError();
        switch(error)
        {
        case EGL_BAD_CONFIG:
            std::cerr << "config is not an EGL frame buffer configuration, or does not support the current rendering API" << std::endl;
            break;
        case EGL_BAD_ATTRIBUTE:
            std::cerr << "attrib_list contains an invalid context attribute or if an attribute is not recognized or out of range" << std::endl;
            break;
        case EGL_BAD_MATCH:
            std::cerr << "From the EGL 1.4 spec, Section 3.7.1:\n"
                         "The OpenGL and OpenGL ES server context state for all sharing contexts "
                         "must exist in a single address space or an EGL_BAD_MATCH error is generated. "
                         "And if share_context was created on a different display than the one referenced by config, [...] then "
                         "an EGL_BAD_MATCH error is generated.\"" << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << error << std::endl;
            break;
        }
        return 0;
    }

    // 6. connect the context to the surface
    if(!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) eglPrintErrorAndExit("eglMakeCurrent");


    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    // Init glew
    GLenum err=glewInit();
    if(err!=GLEW_OK)
    {
        std::cout << "glewInit failed: " << glewGetErrorString(err) << std::endl;            
        std::cout << "Are you sure that you are using GLEW>2.1?" << std::endl;
        std::cout << "Build GLEW2.1 with 'make SYSTEM=linux-egl'" << std::endl;
        return 0;
    } 

    return 1;
}

void EGL::SaveScreenshotPPM(const std::string& filename)
{
    // https://gitlab.kitware.com/third-party/nvpipe/blob/f95cd926b8794bc3ecd50baa97e20f8d56d47c0c/doc/egl-example/egl-example.cpp
    // read pixels
    GLubyte* pixels = new GLubyte[3*pbufferWidth*pbufferHeight];
    glReadPixels(0, 0, pbufferWidth, pbufferHeight, GL_RGB, GL_UNSIGNED_BYTE, (void*)pixels);

    int i, j, cur;
    const size_t format_nchannels = 3;
    FILE *f = fopen(filename.c_str(), "w");
    fprintf(f, "P3\n%d %d\n%d\n", pbufferWidth, pbufferHeight, 255);
    for (i = 0; i < pbufferHeight; i++) {
        for (j = 0; j < pbufferWidth; j++) {
            cur = format_nchannels * ((pbufferHeight - i - 1) * pbufferWidth + j);
            fprintf(f, "%3d %3d %3d ", (pixels)[cur], (pixels)[cur + 1], (pixels)[cur + 2]);
        }
        fprintf(f, "\n");
    }
    fclose(f);
    delete[] pixels;
}

// Texture

void Texture::Init(const char* filename)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Mipmap filters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, n_channels;
    // flip the y-axis
    stbi_set_flip_vertically_on_load(true); 
    unsigned char* data = stbi_load(filename, &width, &height, &n_channels, 0);
    if (data) 
    {
        std::cout << " " << "Loaded image from: " << filename << std::endl;
        std::cout << " " << "Width: " << width << std::endl;
        std::cout << " " << "Height: " << height << std::endl;
        std::cout << " " << "# Channels: " << n_channels << std::endl;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture from " << filename << std::endl;
    }

    stbi_image_free(data);

    state = InternalState::INITIALIZED;
}

// RenderTarget

int RenderTarget::Init(unsigned int _width, unsigned int _height)
{
    width = _width;
    height = _height;

    /////////////////////////
    ///// Framebuffers //////
    /////////////////////////
    //http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    // color texture to render to   
    glGenTextures(1, &color_texture);
    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // position texture to render to   
    glGenTextures(1, &position_texture);
    glBindTexture(GL_TEXTURE_2D, position_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // normal texture
    glGenTextures(1, &normal_texture);
    glBindTexture(GL_TEXTURE_2D, normal_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // uv texture
    glGenTextures(1, &uv_texture);
    glBindTexture(GL_TEXTURE_2D, uv_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, height, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // bary texture
    glGenTextures(1, &bary_texture);
    glBindTexture(GL_TEXTURE_2D, bary_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // vids texture
    glGenTextures(1, &vids_texture);
    glBindTexture(GL_TEXTURE_2D, vids_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // The depth buffer  
    glGenRenderbuffers(1, &depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);

    // Set "color_texture" as our colour attachement #0
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_texture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, position_texture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, normal_texture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, uv_texture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, bary_texture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, vids_texture, 0);

    // Set the list of draw buffers.
    GLenum DrawBuffers[6] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                             GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
                             GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5};
    glDrawBuffers(6, DrawBuffers);

    // Always check that our framebuffer is ok
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        return -1;
    }

    checkCudaErrors(cudaGraphicsGLRegisterImage(&graphics_resource[0], color_texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone));
    checkCudaErrors(cudaGraphicsGLRegisterImage(&graphics_resource[1], position_texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone));
    checkCudaErrors(cudaGraphicsGLRegisterImage(&graphics_resource[2], normal_texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone));
    checkCudaErrors(cudaGraphicsGLRegisterImage(&graphics_resource[3], uv_texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone));
    checkCudaErrors(cudaGraphicsGLRegisterImage(&graphics_resource[4], bary_texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone));
    checkCudaErrors(cudaGraphicsGLRegisterImage(&graphics_resource[5], vids_texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone));

    cudaMalloc((void**)&(buffer[0]), width*height*4*sizeof(float));
    cudaMalloc((void**)&(buffer[1]), width*height*4*sizeof(float));
    cudaMalloc((void**)&(buffer[2]), width*height*4*sizeof(float));
    cudaMalloc((void**)&(buffer[3]), width*height*2*sizeof(float));
    cudaMalloc((void**)&(buffer[4]), width*height*4*sizeof(float));
    cudaMalloc((void**)&(buffer[5]), width*height*4*sizeof(float));

    return 1;
}

void RenderTarget::CopyRenderedTexturesToCUDA(bool copy_to_host)
{
    cudaMemcpyKind copy_mode;
    
    if (copy_to_host) {
        copy_mode = cudaMemcpyDeviceToHost;
    } else {
        copy_mode = cudaMemcpyDeviceToDevice;
    }
     
    checkCudaErrors(cudaGraphicsMapResources(NUM_GRAPHICS_RESOURCES, graphics_resource));
    cudaArray* cuda_array;
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(&cuda_array, graphics_resource[0], 0, 0));
    checkCudaErrors(cudaMemcpy2DFromArray(buffer[0], width*sizeof(float)*4, cuda_array, 0, 0, width*sizeof(float)*4, height, copy_mode));
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(&cuda_array, graphics_resource[1], 0, 0));
    checkCudaErrors(cudaMemcpy2DFromArray(buffer[1], width*sizeof(float)*4, cuda_array, 0, 0, width*sizeof(float)*4, height, copy_mode));
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(&cuda_array, graphics_resource[2], 0, 0));
    checkCudaErrors(cudaMemcpy2DFromArray(buffer[2], width*sizeof(float)*4, cuda_array, 0, 0, width*sizeof(float)*4, height, copy_mode));
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(&cuda_array, graphics_resource[3], 0, 0));
    checkCudaErrors(cudaMemcpy2DFromArray(buffer[3], width*sizeof(float)*2, cuda_array, 0, 0, width*sizeof(float)*2, height, copy_mode));
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(&cuda_array, graphics_resource[4], 0, 0));
    checkCudaErrors(cudaMemcpy2DFromArray(buffer[4], width*sizeof(float)*4, cuda_array, 0, 0, width*sizeof(float)*4, height, copy_mode));
    checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(&cuda_array, graphics_resource[5], 0, 0));
    checkCudaErrors(cudaMemcpy2DFromArray(buffer[5], width*sizeof(float)*4, cuda_array, 0, 0, width*sizeof(float)*4, height, copy_mode));
    checkCudaErrors(cudaGraphicsUnmapResources(NUM_GRAPHICS_RESOURCES, graphics_resource));
}

void RenderTarget::WriteDataToFile(const std::string& filename, float* data, unsigned int tex_id)
{
    size_t format_nchannels = 4;
    switch (tex_id)
    {
        case 0:  break;
        case 1:  break;
        case 2:  break;
        case 3:  format_nchannels = 2; break;
        case 4:  break;
        case 5:  break;
        default: break;
    }

    FreeImage image(width, height, format_nchannels);
    std::memcpy(image.data, data, width*height*format_nchannels*sizeof(float));
    if(!image.SaveImageToFile(filename, true))
    {
        std::cout << "WARNING: unable to write image file:" << filename << std::endl;
    }
}

void RenderTarget::WriteToFile(const std::string& filename, unsigned int tex_id, bool yFlip)
{
    GLuint texture_id = color_texture;
    size_t format_nchannels = 4;
    switch (tex_id)
    {
        case 0:  texture_id = color_texture; break;
        case 1:  texture_id = position_texture; break;
        case 2:  texture_id = normal_texture; break;
        case 3:  texture_id = uv_texture; format_nchannels = 2; break;
        case 4:  texture_id = bary_texture; break;
        case 5:  texture_id = vids_texture; break;
        default: texture_id = color_texture; break;
    }

    glBindTexture(GL_TEXTURE_2D, texture_id);
    //https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGetTexImage.xhtml
    #ifndef NO_FREEIMAGE

        FreeImage image(width, height, format_nchannels);
        if (format_nchannels==2) glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, (void*)image.data);
        if (format_nchannels==3) glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, (void*)image.data);
        if (format_nchannels==4) glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, (void*)image.data);
        if(!image.SaveImageToFile(filename, yFlip))
        {
            std::cout << "WARNING: unable to write image file:" << filename << std::endl;
        }

    #else
        GLubyte* pixels = new GLubyte[3*width*height];
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, (void*)pixels);
        size_t i, j, cur;
        FILE *f = fopen(filename.c_str(), "w");
        fprintf(f, "P3\n%d %d\n%d\n", width, height, 255);
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                cur = format_nchannels * ((height - i - 1) * width + j);
                fprintf(f, "%3d %3d %3d ", (pixels)[cur], (pixels)[cur + 1], (pixels)[cur + 2]);
            }
            fprintf(f, "\n");
        }
        fclose(f);

        delete[] pixels;
    #endif
}

void RenderTarget::Terminate()
{
    for (int i = 0; i < NUM_GRAPHICS_RESOURCES; i++)
    {
        cudaGraphicsUnregisterResource(graphics_resource[i]);
        cudaFree(buffer[i]);
    }
}


void RenderTarget::Use()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height); 
}

void RenderTarget::Clear()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(-1.0f, -1.0f, -1.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void RenderTarget::ClearBack()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(-1.0f, -1.0f, -1.0f, 1.0f);
    glClearDepth(0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
}

// Shader

int Shader::LoadShader(const char  *shader_source, GLenum type)
{
    if (type == GL_VERTEX_SHADER)   std::cout << "- load vertex shader" << std::endl;
    if (type == GL_GEOMETRY_SHADER) std::cout << "- load geometry shader" << std::endl;
    if (type == GL_FRAGMENT_SHADER) std::cout << "- load fragment shader" << std::endl;
    if (type == GL_COMPUTE_SHADER) std::cout << "- load compute shader" << std::endl;
    

    // create shader
    shader = glCreateShader( type );
    if(shader == 0)
    {
        std::cout << "ERROR: failed to create a shader (glCreateShader)" << std::endl;
        return 0;
    }
    CheckError();

    // load shader source
    glShaderSource(shader , 1 , &shader_source , NULL);
    CheckError();

    // compile shader
    glCompileShader(shader);
    CheckError();

    // print compilation log
    return print_shader_info_log();
}

int Shader::LoadShaderFromFile(const std::string& filename, GLenum type, const std::vector<std::string>& defines)
{
    std::ifstream shader_file(filename);

    if(!shader_file.is_open())
    {
        std::cout << "ERROR: unable to open shader file: " << filename << std::endl;
        return 0;
    }

    std::string shader_src((std::istreambuf_iterator<char>(shader_file)), std::istreambuf_iterator<char>());
    shader_file.close();

    std::size_t second_line = shader_src.find(std::string("\n")) + 1;

    for (const auto& define : defines)
    {
        shader_src.insert(second_line, std::string("#define ").append(define).append("\n"));
    }

    //std::cout << shader_src << std::endl;

    return LoadShader(shader_src.c_str(), type);
}

int Shader::print_shader_info_log()
{
    GLint  length;
    glGetShaderiv ( shader , GL_INFO_LOG_LENGTH , &length );
    CheckError();
    //std::cout << "GL_INFO_LOG_LENGTH:" << length << std::endl;

    if (length)
    {
        char* buffer  =  new char [ length ];
        glGetShaderInfoLog (shader, length, NULL, buffer);
        std::cout << "glGetShaderInfoLog:\n" <<  buffer << std::endl;

        delete [] buffer;
    
        // compile status
        GLint success;    
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        CheckError();
        if (success != GL_TRUE)
        {
            std::cout << "ERROR: shader compilation failed" << std::endl;
            return 0;
        }
    }
    return 1;
}

// ShaderProgram

int ShaderProgram::Init(Shader& vertexShader, Shader& fragmentShader)
{
    std::cout << "- create shader program" << std::endl;
    shaderProgram = glCreateProgram();      // create program object
    glAttachShader(shaderProgram, vertexShader.GetID());        
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: attaching vertex shader failed!" << std::endl;
        return -1;
    }
    glAttachShader(shaderProgram, fragmentShader.GetID());
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: attaching fragment shader failed!" << std::endl;
        return -1;
    }

    glLinkProgram(shaderProgram); // link the program
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: linking shader program failed!" << std::endl;
        return -1;
    }
    glUseProgram(shaderProgram);  // and select it for usage
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: use shader program failed!" << std::endl;
        return -1;
    }

    return 1;
}

int ShaderProgram::Init(Shader& vertexShader, Shader& geometryShader, Shader& fragmentShader)
{
    std::cout << "- create shader program" << std::endl;
    shaderProgram = glCreateProgram();      // create program object
    glAttachShader(shaderProgram, vertexShader.GetID());
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: attaching vertex shader failed!" << std::endl;
        return -1;
    }
    glAttachShader(shaderProgram, geometryShader.GetID());
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: attaching geometry shader failed!" << std::endl;
        return -1;
    }
    glAttachShader(shaderProgram, fragmentShader.GetID());
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: attaching fragment shader failed!" << std::endl;
        return -1;
    }

    glLinkProgram(shaderProgram); // link the program
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: linking shader program failed!" << std::endl;
        return -1;
    }
    glUseProgram(shaderProgram);  // and select it for usage
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: use shader program failed!" << std::endl;
        return -1;
    }

    return 1;
}

int ShaderProgram::Init(Shader& computeShader)
{
    std::cout << "- create shader program" << std::endl;
    shaderProgram = glCreateProgram();      // create program object
    glAttachShader(shaderProgram, computeShader.GetID());        
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: attaching vertex shader failed!" << std::endl;
        return -1;
    }

    glLinkProgram(shaderProgram); // link the program
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: linking shader program failed!" << std::endl;
        return -1;
    }
    glUseProgram(shaderProgram);  // and select it for usage
    if(glGetError() != GL_NO_ERROR)
    {
        std::cout << "ERROR: use shader program failed!" << std::endl;
        return -1;
    }

    return 1;
}

int ShaderProgram::Init(const std::string& filename_vertexShader, const std::string& filename_fragmentShader, const std::vector<std::string>& defines)
{
    Shader vertexShader, fragmentShader;
    if(!vertexShader.LoadShaderFromFile(filename_vertexShader, GL_VERTEX_SHADER, defines))
    {
        std::cout << "ERROR: loading vertex shader failed" << std::endl;
        return -1;
    }
    if(!fragmentShader.LoadShaderFromFile(filename_fragmentShader, GL_FRAGMENT_SHADER, defines))
    {
        std::cout << "ERROR: loading fragment shader failed" << std::endl;
        return -1;
    }
    return Init(vertexShader, fragmentShader);
}

int ShaderProgram::Init(const std::string& filename_vertexShader, const std::string& filename_geometryShader, const std::string& filename_fragmentShader, const std::vector<std::string>& defines)
{
    Shader vertexShader, geometryShader, fragmentShader;
    if(!vertexShader.LoadShaderFromFile(filename_vertexShader, GL_VERTEX_SHADER, defines))
    {
        std::cout << "ERROR: loading vertex shader failed" << std::endl;
        return -1;
    }
    if(!geometryShader.LoadShaderFromFile(filename_geometryShader, GL_GEOMETRY_SHADER, defines))
    {
        std::cout << "ERROR: loading geometry shader failed" << std::endl;
        return -1;
    }
    if(!fragmentShader.LoadShaderFromFile(filename_fragmentShader, GL_FRAGMENT_SHADER, defines))
    {
        std::cout << "ERROR: loading fragment shader failed" << std::endl;
        return -1;
    }
    return Init(vertexShader, geometryShader, fragmentShader);
}

int ShaderProgram::Init(const std::string& filename_computeShader, const std::vector<std::string>& defines)
{
    Shader computeShader;
    if(!computeShader.LoadShaderFromFile(filename_computeShader, GL_COMPUTE_SHADER, defines))
    {
        std::cout << "ERROR: loading compute shader failed" << std::endl;
        return -1;
    }
    return Init(computeShader);
}

// Transformation

void Transformation::SetPerspectiveProjection(float fovX, float fovY, float cX, float cY, float near, float far)
{
    projection = {
        2.0f * fovX, 0.0, cX - 0.5f, 0.0, 
        0.0, 2.0f * fovY, cY - 0.5f, 0.0, 
        0.0, 0.0, (far + near) / (near - far), (2 * far * near) / (near - far),
        0.0, 0.0, -1.0, 0.0
    };
}

void Transformation::SetWeakPerspectiveProjection(float fx, float fy, float cx, float cy)
{
    projection = {
        fx, 0.0, 0.0, cx * fx,
        0.0, fy, 0.0, -cy * fy,
        0.0, 0.0, -1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
}

void Transformation::SetPinholeProjection(float fx, float fy, float cx, float cy, float near, float far, float width, float height)
{
    projection = {
        2.0f * fx / width, 0.0, 1.0f - 2.0f * cx / width, 0.0,
        0.0, 2.0f * fy / height, 2.0f * cy / height - 1.0f, 0.0,
        0.0, 0.0, (far + near) / (near - far), (2 * far * near) / (near - far),
        0.0, 0.0, -1.0, 0.0
    };
}

void Transformation::SetPinholeZeroOpticalCenterProjection(float fx, float fy, float cx, float cy, float near, float far, float width, float height)
{
    projection = {
        2.0f * fx / width, 0.0, 0.0, 0.0,
        0.0, 2.0f * fy / height, 0.0, 0.0,
        0.0, 0.0, (far + near) / (near - far), (2 * far * near) / (near - far),
        0.0, 0.0, -1.0, 0.0
    };
}

void Transformation::SetIdentityProjection()
{
    projection = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
}

// Mesh

int Mesh::LoadObjFile(const std::string& filename, float scale)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::vector<Vertex> vertices;
    vertices.reserve(10000);
    std::vector<unsigned int> indices;
    indices.reserve(10000);

    std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto &index : shape.mesh.indices) {
            Vertex v{};
            v.x = attrib.vertices[3 * index.vertex_index + 0] * scale;
            v.y = attrib.vertices[3 * index.vertex_index + 1] * scale;
            v.z = attrib.vertices[3 * index.vertex_index + 2] * scale;
            v.mask = 1.0f;

            if (!attrib.colors.empty()) {
                v.r = attrib.colors[3 * index.vertex_index + 0];
                v.g = attrib.colors[3 * index.vertex_index + 1];
                v.b = attrib.colors[3 * index.vertex_index + 2];
                v.a = 1.0;
            }

            if (!attrib.normals.empty()) {
                v.nx = attrib.normals[3 * index.vertex_index + 0];
                v.ny = attrib.normals[3 * index.vertex_index + 1];
                v.nz = attrib.normals[3 * index.vertex_index + 2];
            }

            if (!attrib.texcoords.empty()) {
                v.u = attrib.texcoords[2 * index.texcoord_index + 0];
                v.v = attrib.texcoords[2 * index.texcoord_index + 1];
            }

            //std::cout << "vertex: " << v.x << " " << v.y << " " << v.z << std::endl;

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
            }

            indices.push_back(uniqueVertices[v]);
            //std::cout << "index: " << indices[indices.size() - 1] << std::endl;
        }
    }

    Init(vertices.data(), vertices.size(), indices.data(), indices.size() / 3);

    return 0;
}

int Mesh::LoadOffFile(const std::string& filename, float scale)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    std::ifstream file(filename);
    if(!file.is_open())
    {
        std::cout << "Error: unable to open mesh file: " << filename << std::endl;
        return -1;
    }

    
    std::string line;
    std::getline(file, line);

    std::string format;
    {
        std::stringstream line_ss;
        line_ss << line;
        line_ss >> format;
    }

    if(format != "STCOFF" && format != "COFF" && format != "STOFF")
    {
        std::cout << "Error: unsupported mesh format: " << format << std::endl;
        return -1;
    }

    unsigned int n_vertices, n_faces, dump;
    {
        std::stringstream line_ss;
        std::getline(file, line);
        line_ss << line;
        line_ss >> n_vertices >> n_faces >> dump;
    }
    //std::cout << "format: " << format << std::endl;
    //std::cout << "n_vertices: " << n_vertices << std::endl;
    //std::cout << "n_faces: " << n_faces << std::endl;

    for(unsigned int i=0; i<n_vertices; ++i)
    {
        std::string line;
        std::getline(file, line);
        std::stringstream line_ss;
        line_ss << line;

        Vertex v;
        //-57239 42965.7 80410.1 183 135 107 255 0.287456 0.613019
        line_ss >> v.x >> v.y >> v.z;
        if(format == "STCOFF" || format == "COFF") line_ss >> v.r >> v.g >> v.b >> v.a;
        if(format == "STCOFF" || format == "STOFF") line_ss >> v.u >> v.v;
        v.x *= scale;
        v.y *= scale;
        v.z *= scale;
        v.r /= 255.0f;
        v.g /= 255.0f;
        v.b /= 255.0f;
        v.a /= 255.0f;
        v.nx = 0.0f;
        v.ny = 0.0f;
        v.nz = 1.0f;
        v.mask = 1.0f;

        vertices.push_back(v);
    }

    for (unsigned int i=0; i<n_faces; ++i)
    {
        std::string line;
        std::getline(file, line);
        std::stringstream line_ss;
        line_ss << line;

        unsigned int dump, i0,i1,i2;
        // 3 40923 40897 40924
        line_ss >> dump >> i0 >> i1 >> i2;
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
    }


    file.close();
    Init(vertices.data(), vertices.size(), indices.data(), indices.size() / 3);

    return 0;
}

void Mesh::Terminate()
{
    if (initialized)
    {
        //code=4(cudaErrorCudartUnloading) when executed in destructor
        checkCudaErrors(cudaGraphicsUnregisterResource(VertexVBORes));
        const GLuint buffers[2] = {VertexVBOID, IndexVBOID};
        glDeleteBuffers(2, buffers);
        glDeleteVertexArrays(1, &vao);
        initialized = false;
    }
}

void Mesh::Init(OpenGL::Vertex* vertex_data, unsigned int n_vertices, unsigned int* indices, unsigned int n_faces, bool vertex_data_on_cuda)
{
    std::cout << "Initialize mesh (" << n_vertices << " | " << n_faces << ")" << std::endl;

    glGenBuffers(1, &VertexVBOID);
    glBindBuffer(GL_ARRAY_BUFFER, VertexVBOID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(OpenGL::Vertex)*n_vertices, nullptr, GL_DYNAMIC_COPY);
    checkCudaErrors(cudaGraphicsGLRegisterBuffer(&VertexVBORes, VertexVBOID, cudaGraphicsRegisterFlagsNone));

    checkCudaErrors(cudaGraphicsMapResources(1, &VertexVBORes));
    float* vboPtr;
    size_t size;
    checkCudaErrors(cudaGraphicsResourceGetMappedPointer((void**)&vboPtr, &size, VertexVBORes));

    if (vertex_data_on_cuda) {
        checkCudaErrors(cudaMemcpy((void*)vboPtr, (void*)vertex_data, size, cudaMemcpyDeviceToDevice));
    } else {
        checkCudaErrors(cudaMemcpy((void*)vboPtr, (void*)vertex_data, size, cudaMemcpyHostToDevice));
    }

    checkCudaErrors(cudaGraphicsUnmapResources(1, &VertexVBORes));
    //checkCudaErrors(cudaStreamSynchronize(0));

    glGenBuffers(1, &IndexVBOID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*3*n_faces, indices, GL_STATIC_DRAW);

    //https://stackoverflow.com/questions/58022707/glvertexattribpointer-raise-gl-invalid-operation-version-330
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    this->n_vertices = n_vertices;
    this->n_faces = n_faces;
    this->vertex_data_on_cuda = vertex_data_on_cuda;
    initialized = true;
}

void Mesh::Update(OpenGL::Vertex* vertex_data, unsigned int n_vertices, bool vertex_data_on_cuda)
{
    checkCudaErrors(cudaGraphicsMapResources(1, &VertexVBORes));
    float* vboPtr;
    size_t size;
    checkCudaErrors(cudaGraphicsResourceGetMappedPointer((void**)&vboPtr, &size, VertexVBORes));
     
    if (vertex_data_on_cuda) {
        checkCudaErrors(cudaMemcpy((void*)vboPtr, (void*)vertex_data, size, cudaMemcpyDeviceToDevice));
    } else {
        checkCudaErrors(cudaMemcpy((void*)vboPtr, (void*)vertex_data, size, cudaMemcpyHostToDevice));
    }

    checkCudaErrors(cudaGraphicsUnmapResources(1, &VertexVBORes));
    //checkCudaErrors(cudaStreamSynchronize(0));
}

int Mesh::Render(GLint position_loc, GLint normal_loc, GLint color_loc, GLint uv_loc, GLint mask_loc)
{
    // vertex array object
    glBindVertexArray(vao);

    if(verbose) std::cout << "glBindBuffer" << std::endl;
    glBindBuffer(GL_ARRAY_BUFFER, VertexVBOID);
    OpenGL::CheckError();

    if(verbose) std::cout << "glEnableVertexAttribArray" << std::endl;
    // position
    if (position_loc >= 0)
    {
        glEnableVertexAttribArray(position_loc);
        glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, sizeof(OpenGL::Vertex), BUFFER_OFFSET(0));
        OpenGL::CheckError();
    }
    else
    {
        std::cout << "ERROR: unable to render, position_loc not set!" << std::endl;
        return -1;
    }

    // normal
    if (normal_loc >= 0)
    {
        glEnableVertexAttribArray(normal_loc);
        glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, sizeof(OpenGL::Vertex), BUFFER_OFFSET(3*sizeof(float)));
        OpenGL::CheckError();
    }
    // color
    if (color_loc >= 0)
    {
        glEnableVertexAttribArray(color_loc);
        glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, sizeof(OpenGL::Vertex), BUFFER_OFFSET(6*sizeof(float)));
        OpenGL::CheckError();
    }

    // uv
    if (uv_loc >= 0)
    {
        glEnableVertexAttribArray(uv_loc);
        glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, sizeof(OpenGL::Vertex), BUFFER_OFFSET(10*sizeof(float)));
        OpenGL::CheckError();
    }

    // mask
    if (mask_loc >= 0)
    {
        glEnableVertexAttribArray(mask_loc);
        glVertexAttribPointer(mask_loc, 1, GL_FLOAT, GL_FALSE, sizeof(OpenGL::Vertex), BUFFER_OFFSET(12*sizeof(float)));
        OpenGL::CheckError();
    }


    // index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOID);
    OpenGL::CheckError();
    // To render, we can either use glDrawElements or glDrawRangeElements
    // The is the number of indices. 3 indices needed to make a single triangle
    if(verbose) std::cout << "glDrawElements" << std::endl;
    glDrawElements(GL_TRIANGLES, 3*n_faces, GL_UNSIGNED_INT, BUFFER_OFFSET(0));    // The starting point of the IBO 
    OpenGL::CheckError();

    // 0 and 3 are the first and last vertices
    // glDrawRangeElements(GL_TRIANGLES, 0, 3, 3, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));    //The starting point of the IBO
    // glDrawRangeElements may or may not give a performance advantage over glDrawElements

    return 0;
}

} // namespace OpenGL