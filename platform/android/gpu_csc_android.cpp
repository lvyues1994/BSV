#include "bsv/gpu_csc.h"

#ifdef __ANDROID__

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <android/hardware_buffer.h>

#include <cstring>
#include <vector>

#include "bsv/gpu_csc_shader.h"

namespace bsv {

namespace {

using EglCreateImage = EGLImageKHR (*)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
using EglDestroyImage = EGLBoolean (*)(EGLDisplay, EGLImageKHR);
using GlEglImageTargetTex2D = void (*)(GLenum, GLeglImageOES);

constexpr EGLenum kEglImageTarget = EGL_NATIVE_BUFFER_ANDROID;
constexpr EGLint kEglImageAttribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return 0;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint LinkProgram(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    if (program == 0) {
        return 0;
    }
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool CreateEglContextWithVersion(EGLDisplay display, int version, EGLConfig* out_config,
                                 EGLSurface* out_surface, EGLContext* out_context) {
    if (out_config == nullptr || out_surface == nullptr || out_context == nullptr) {
        return false;
    }
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, version == 3 ? EGL_OPENGL_ES3_BIT_KHR : EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint num_configs = 0;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
        return false;
    }
    const EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        return false;
    }
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, version,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        eglDestroySurface(display, surface);
        return false;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        return false;
    }
    *out_config = config;
    *out_surface = surface;
    *out_context = context;
    return true;
}

std::vector<uint8_t> CopyPlaneContiguous(const uint8_t* src, uint32_t width, uint32_t height, uint32_t stride) {
    std::vector<uint8_t> contiguous(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; ++y) {
        std::memcpy(contiguous.data() + static_cast<size_t>(y) * width,
                    src + static_cast<size_t>(y) * stride,
                    width);
    }
    return contiguous;
}

}  // namespace

AndroidGpuCscConverter::AndroidGpuCscConverter() = default;

AndroidGpuCscConverter::~AndroidGpuCscConverter() {
    Shutdown();
}

BsvError AndroidGpuCscConverter::Initialize(const CscConfig& config) {
    if (config.output_format != PixelFormat::kRGBA8888) {
        return BsvError::kNotSupported;
    }
    if (config.input_format != PixelFormat::kNV12 && config.input_format != PixelFormat::kNV21) {
        return BsvError::kNotSupported;
    }
    config_ = config;
    BsvError status = CreateEglContext();
    if (status != BsvError::kOk) {
        return status;
    }
    status = CreateShaders();
    if (status != BsvError::kOk) {
        DestroyEglContext();
        return status;
    }
    initialized_ = true;
    return BsvError::kOk;
}

BsvError AndroidGpuCscConverter::Start() {
    if (!initialized_) {
        return BsvError::kNotInitialized;
    }
    return BsvError::kOk;
}

BsvError AndroidGpuCscConverter::Stop() {
    return BsvError::kOk;
}

void AndroidGpuCscConverter::Shutdown() {
    DestroyShaders();
    DestroyEglContext();
    initialized_ = false;
}

BsvError AndroidGpuCscConverter::ConvertFrame(const IBuffer& src, IBuffer& dst) {
    if (!initialized_) {
        return BsvError::kNotInitialized;
    }
    if (src.GetDesc().format != config_.input_format || dst.GetDesc().format != config_.output_format) {
        return BsvError::kInvalidArgument;
    }
    return DrawToOutput(src, dst);
}

BsvError AndroidGpuCscConverter::CreateEglContext() {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
        return BsvError::kInternal;
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLConfig config = nullptr;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    if (CreateEglContextWithVersion(display, 3, &config, &surface, &context)) {
        gles_version_ = 3;
    } else if (CreateEglContextWithVersion(display, 2, &config, &surface, &context)) {
        gles_version_ = 2;
    } else {
        eglTerminate(display);
        return BsvError::kInternal;
    }
    egl_display_ = display;
    egl_surface_ = surface;
    egl_context_ = context;
    egl_config_ = config;
    return BsvError::kOk;
}

void AndroidGpuCscConverter::DestroyEglContext() {
    EGLDisplay display = static_cast<EGLDisplay>(egl_display_);
    EGLSurface surface = static_cast<EGLSurface>(egl_surface_);
    EGLContext context = static_cast<EGLContext>(egl_context_);
    if (display != nullptr && display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
        }
        eglTerminate(display);
    }
    egl_display_ = nullptr;
    egl_surface_ = nullptr;
    egl_context_ = nullptr;
    egl_config_ = nullptr;
}

BsvError AndroidGpuCscConverter::CreateShaders() {
    GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, kVertexShaderSource);
    if (vertex_shader == 0) {
        return BsvError::kInternal;
    }
    GLuint fragment_shader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return BsvError::kInternal;
    }
    GLuint program = LinkProgram(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (program == 0) {
        return BsvError::kInternal;
    }
    program_ = program;
    attr_pos_ = glGetAttribLocation(program_, "aPos");
    attr_uv_ = glGetAttribLocation(program_, "aTex");
    uni_y_tex_ = glGetUniformLocation(program_, "uYTex");
    uni_uv_tex_ = glGetUniformLocation(program_, "uUVTex");
    uni_is_nv21_ = glGetUniformLocation(program_, "uIsNV21");

    glGenTextures(1, &tex_y_);
    glGenTextures(1, &tex_uv_);
    glGenTextures(1, &output_tex_);
    glGenFramebuffers(1, &fbo_);
    return BsvError::kOk;
}

void AndroidGpuCscConverter::DestroyShaders() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    if (tex_y_ != 0) {
        glDeleteTextures(1, &tex_y_);
        tex_y_ = 0;
    }
    if (tex_uv_ != 0) {
        glDeleteTextures(1, &tex_uv_);
        tex_uv_ = 0;
    }
    if (output_tex_ != 0) {
        glDeleteTextures(1, &output_tex_);
        output_tex_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (egl_image_ != nullptr && egl_display_ != nullptr) {
        auto destroy_image = reinterpret_cast<EglDestroyImage>(eglGetProcAddress("eglDestroyImageKHR"));
        if (destroy_image != nullptr) {
            destroy_image(static_cast<EGLDisplay>(egl_display_), static_cast<EGLImageKHR>(egl_image_));
        }
        egl_image_ = nullptr;
        output_handle_ = nullptr;
    }
}

BsvError AndroidGpuCscConverter::ConfigureTextures(uint32_t width, uint32_t height) {
    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, static_cast<GLint>(width), static_cast<GLint>(height), 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, tex_uv_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, static_cast<GLint>(width / 2),
                 static_cast<GLint>(height / 2), 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);
    return BsvError::kOk;
}

BsvError AndroidGpuCscConverter::EnsureOutputTarget(IBuffer& dst) {
    const PlatformHandle* handle = dst.GetPlatformHandle();
    if (handle == nullptr || handle->type != PlatformHandleType::kAndroidHardwareBuffer || handle->handle == nullptr) {
        return BsvError::kInvalidArgument;
    }
    if (output_handle_ == handle->handle && egl_image_ != nullptr) {
        return BsvError::kOk;
    }
    if (egl_image_ != nullptr && egl_display_ != nullptr) {
        auto destroy_image = reinterpret_cast<EglDestroyImage>(eglGetProcAddress("eglDestroyImageKHR"));
        if (destroy_image != nullptr) {
            destroy_image(static_cast<EGLDisplay>(egl_display_), static_cast<EGLImageKHR>(egl_image_));
        }
        egl_image_ = nullptr;
    }
    auto create_image = reinterpret_cast<EglCreateImage>(eglGetProcAddress("eglCreateImageKHR"));
    if (create_image == nullptr) {
        return BsvError::kNotSupported;
    }
    AHardwareBuffer* buffer = static_cast<AHardwareBuffer*>(handle->handle);
    EGLClientBuffer client_buffer = eglGetNativeClientBufferANDROID(buffer);
    if (client_buffer == nullptr) {
        return BsvError::kInternal;
    }
    EGLImageKHR image = create_image(static_cast<EGLDisplay>(egl_display_),
                                     EGL_NO_CONTEXT,
                                     kEglImageTarget,
                                     client_buffer,
                                     kEglImageAttribs);
    if (image == EGL_NO_IMAGE_KHR) {
        return BsvError::kInternal;
    }
    egl_image_ = image;
    output_handle_ = handle->handle;

    glBindTexture(GL_TEXTURE_2D, output_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    auto image_target = reinterpret_cast<GlEglImageTargetTex2D>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (image_target == nullptr) {
        return BsvError::kNotSupported;
    }
    image_target(GL_TEXTURE_2D, static_cast<GLeglImageOES>(egl_image_));
    glBindTexture(GL_TEXTURE_2D, 0);
    return BsvError::kOk;
}

BsvError AndroidGpuCscConverter::UploadInput(const IBuffer& src) {
    const BufferDesc& desc = src.GetDesc();
    BufferMapping mapping;
    IBuffer& mutable_src = const_cast<IBuffer&>(src);
    if (mutable_src.Map(BufferAccessMode::kRead, &mapping) != BsvError::kOk) {
        return BsvError::kInvalidArgument;
    }
    const auto* src_bytes = static_cast<const uint8_t*>(mapping.data);
    const size_t y_plane = static_cast<size_t>(desc.stride) * desc.height;
    const uint8_t* uv_plane = src_bytes + y_plane;
    const bool contiguous = desc.stride == desc.width;

    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (contiguous) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLint>(desc.width), static_cast<GLint>(desc.height),
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, src_bytes);
    } else {
        std::vector<uint8_t> y_plane_contiguous = CopyPlaneContiguous(src_bytes, desc.width, desc.height, desc.stride);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLint>(desc.width), static_cast<GLint>(desc.height),
                        GL_LUMINANCE, GL_UNSIGNED_BYTE, y_plane_contiguous.data());
    }
    glBindTexture(GL_TEXTURE_2D, tex_uv_);
    if (contiguous) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLint>(desc.width / 2),
                        static_cast<GLint>(desc.height / 2), GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, uv_plane);
    } else {
        const uint32_t uv_height = desc.height / 2;
        std::vector<uint8_t> uv_plane_contiguous = CopyPlaneContiguous(uv_plane, desc.width, uv_height, desc.stride);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLint>(desc.width / 2),
                        static_cast<GLint>(desc.height / 2), GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                        uv_plane_contiguous.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    mutable_src.Unmap(&mapping);
    return BsvError::kOk;
}

BsvError AndroidGpuCscConverter::DrawToOutput(const IBuffer& src, IBuffer& dst) {
    const BufferDesc& desc = src.GetDesc();
    const BufferDesc& out_desc = dst.GetDesc();
    if (out_desc.width != desc.width || out_desc.height != desc.height) {
        return BsvError::kInvalidArgument;
    }
    if (surface_width_ != static_cast<int>(desc.width) || surface_height_ != static_cast<int>(desc.height)) {
        surface_width_ = static_cast<int>(desc.width);
        surface_height_ = static_cast<int>(desc.height);
        ConfigureTextures(desc.width, desc.height);
    }
    BsvError status = EnsureOutputTarget(dst);
    if (status != BsvError::kOk) {
        return status;
    }
    status = UploadInput(src);
    if (status != BsvError::kOk) {
        return status;
    }

    glUseProgram(program_);
    glViewport(0, 0, surface_width_, surface_height_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glUniform1i(uni_y_tex_, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_uv_);
    glUniform1i(uni_uv_tex_, 1);
    glUniform1i(uni_is_nv21_, config_.input_format == PixelFormat::kNV21 ? 1 : 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_tex_, 0);

    glEnableVertexAttribArray(static_cast<GLuint>(attr_pos_));
    glEnableVertexAttribArray(static_cast<GLuint>(attr_uv_));
    glVertexAttribPointer(static_cast<GLuint>(attr_pos_), 2, GL_FLOAT, GL_FALSE, 0, kQuadVertices);
    glVertexAttribPointer(static_cast<GLuint>(attr_uv_), 2, GL_FLOAT, GL_FALSE, 0, kQuadTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(static_cast<GLuint>(attr_pos_));
    glDisableVertexAttribArray(static_cast<GLuint>(attr_uv_));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFinish();
    return BsvError::kOk;
}

}  // namespace bsv

#endif  // __ANDROID__
