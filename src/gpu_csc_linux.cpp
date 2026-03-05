#include "bsv/gpu_csc.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace bsv {

namespace {

constexpr char kVertexShaderSource[] =
    "attribute vec2 aPos;"
    "attribute vec2 aTex;"
    "varying vec2 vTex;"
    "void main() {"
    "  vTex = aTex;"
    "  gl_Position = vec4(aPos, 0.0, 1.0);"
    "}";

constexpr char kFragmentShaderSource[] =
    "precision mediump float;"
    "varying vec2 vTex;"
    "uniform sampler2D uYTex;"
    "uniform sampler2D uUVTex;"
    "uniform int uIsNV21;"
    "vec3 yuvToRgb(float y, float u, float v) {"
    "  float c = y - 0.0625;"
    "  float d = u - 0.5;"
    "  float e = v - 0.5;"
    "  float r = 1.1643 * c + 1.5958 * e;"
    "  float g = 1.1643 * c - 0.3917 * d - 0.8129 * e;"
    "  float b = 1.1643 * c + 2.017 * d;"
    "  return clamp(vec3(r, g, b), 0.0, 1.0);"
    "}"
    "void main() {"
    "  float y = texture2D(uYTex, vTex).r;"
    "  vec2 uv = texture2D(uUVTex, vTex).ra;"
    "  float u = uIsNV21 == 1 ? uv.y : uv.x;"
    "  float v = uIsNV21 == 1 ? uv.x : uv.y;"
    "  vec3 rgb = yuvToRgb(y, u, v);"
    "  gl_FragColor = vec4(rgb, 1.0);"
    "}";

constexpr GLfloat kQuadVertices[] = {
    -1.0f, -1.0f,
    1.0f,  -1.0f,
    -1.0f, 1.0f,
    1.0f,  1.0f,
};

constexpr GLfloat kQuadTexCoords[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 0.0f,
};

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

uint8_t ClampToByte(float value) {
    value = std::min(255.0f, std::max(0.0f, value));
    return static_cast<uint8_t>(value + 0.5f);
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

void ConvertYuvToRgba(uint8_t y, uint8_t u, uint8_t v, uint8_t* rgba) {
    float yf = static_cast<float>(y) - 16.0f;
    float uf = static_cast<float>(u) - 128.0f;
    float vf = static_cast<float>(v) - 128.0f;
    float r = 1.164f * yf + 1.596f * vf;
    float g = 1.164f * yf - 0.392f * uf - 0.813f * vf;
    float b = 1.164f * yf + 2.017f * uf;
    rgba[0] = ClampToByte(r);
    rgba[1] = ClampToByte(g);
    rgba[2] = ClampToByte(b);
    rgba[3] = 255;
}

std::vector<uint8_t> ConvertNvToRgba(const IBuffer& src, bool is_nv21) {
    const BufferDesc& desc = src.GetDesc();
    const uint32_t width = desc.width;
    const uint32_t height = desc.height;
    const uint32_t stride = desc.stride;

    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
    BufferMapping mapping;
    IBuffer& mutable_src = const_cast<IBuffer&>(src);
    if (mutable_src.Map(BufferAccessMode::kRead, &mapping) != BsvError::kOk) {
        return rgba;
    }
    const auto* src_bytes = static_cast<const uint8_t*>(mapping.data);
    const size_t y_plane = static_cast<size_t>(stride) * height;
    const uint8_t* uv_plane = src_bytes + y_plane;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t y_value = src_bytes[y * stride + x];
            const uint32_t uv_x = x / 2;
            const uint32_t uv_y = y / 2;
            const size_t uv_index = static_cast<size_t>(uv_y) * stride + uv_x * 2;
            uint8_t u = uv_plane[uv_index];
            uint8_t v = uv_plane[uv_index + 1];
            if (is_nv21) {
                std::swap(u, v);
            }
            uint8_t* dst = &rgba[(static_cast<size_t>(y) * width + x) * 4];
            ConvertYuvToRgba(y_value, u, v, dst);
        }
    }
    mutable_src.Unmap(&mapping);
    return rgba;
}

}  // namespace

LinuxGpuCscConverter::LinuxGpuCscConverter() = default;

LinuxGpuCscConverter::~LinuxGpuCscConverter() {
    Shutdown();
}

BsvError LinuxGpuCscConverter::Initialize(const CscConfig& config) {
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

BsvError LinuxGpuCscConverter::Start() {
    if (!initialized_) {
        return BsvError::kNotInitialized;
    }
    return BsvError::kOk;
}

BsvError LinuxGpuCscConverter::Stop() {
    return BsvError::kOk;
}

void LinuxGpuCscConverter::Shutdown() {
    DestroyShaders();
    DestroyEglContext();
    initialized_ = false;
}

BsvError LinuxGpuCscConverter::ConvertFrame(const IBuffer& src, IBuffer& dst) {
    if (!initialized_) {
        return BsvError::kNotInitialized;
    }
    if (src.GetDesc().format != config_.input_format || dst.GetDesc().format != config_.output_format) {
        return BsvError::kInvalidArgument;
    }
    return DrawAndReadback(src, dst);
}

BsvError LinuxGpuCscConverter::CreateEglContext() {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
        using GetPlatformDisplay = EGLDisplay (*)(EGLenum, void*, const EGLint*);
        auto get_platform_display = reinterpret_cast<GetPlatformDisplay>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (get_platform_display != nullptr) {
            display = get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        }
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
            return BsvError::kInternal;
        }
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint num_configs = 0;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
        eglTerminate(display);
        return BsvError::kInternal;
    }
    const EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        eglTerminate(display);
        return BsvError::kInternal;
    }
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return BsvError::kInternal;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return BsvError::kInternal;
    }
    egl_display_ = display;
    egl_surface_ = surface;
    egl_context_ = context;
    egl_config_ = config;
    return BsvError::kOk;
}

void LinuxGpuCscConverter::DestroyEglContext() {
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

BsvError LinuxGpuCscConverter::CreateShaders() {
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
    glGenTextures(1, &color_tex_);
    glGenFramebuffers(1, &fbo_);
    return BsvError::kOk;
}

void LinuxGpuCscConverter::DestroyShaders() {
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
    if (color_tex_ != 0) {
        glDeleteTextures(1, &color_tex_);
        color_tex_ = 0;
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}

BsvError LinuxGpuCscConverter::ConfigureTextures(const IBuffer& src) {
    const BufferDesc& desc = src.GetDesc();
    const GLint width = static_cast<GLint>(desc.width);
    const GLint height = static_cast<GLint>(desc.height);

    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, tex_uv_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, width / 2, height / 2, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, color_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);
    return BsvError::kOk;
}

BsvError LinuxGpuCscConverter::DrawAndReadback(const IBuffer& src, IBuffer& dst) {
    const BufferDesc& desc = src.GetDesc();
    const BufferDesc& out_desc = dst.GetDesc();
    if (out_desc.width != desc.width || out_desc.height != desc.height) {
        return BsvError::kInvalidArgument;
    }
    if (surface_width_ != static_cast<int>(desc.width) || surface_height_ != static_cast<int>(desc.height)) {
        surface_width_ = static_cast<int>(desc.width);
        surface_height_ = static_cast<int>(desc.height);
        ConfigureTextures(src);
    }

    BufferMapping src_mapping;
    IBuffer& mutable_src = const_cast<IBuffer&>(src);
    if (mutable_src.Map(BufferAccessMode::kRead, &src_mapping) != BsvError::kOk) {
        return BsvError::kInvalidArgument;
    }
    const auto* src_bytes = static_cast<const uint8_t*>(src_mapping.data);
    const size_t y_plane = static_cast<size_t>(desc.stride) * desc.height;
    const uint8_t* uv_plane = src_bytes + y_plane;

    glUseProgram(program_);
    glViewport(0, 0, surface_width_, surface_height_);
    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (desc.stride == desc.width) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface_width_, surface_height_, GL_LUMINANCE, GL_UNSIGNED_BYTE, src_bytes);
    } else {
        std::vector<uint8_t> y_plane_contiguous = CopyPlaneContiguous(src_bytes, desc.width, desc.height, desc.stride);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface_width_, surface_height_, GL_LUMINANCE, GL_UNSIGNED_BYTE, y_plane_contiguous.data());
    }
    glBindTexture(GL_TEXTURE_2D, tex_uv_);
    if (desc.stride == desc.width) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface_width_ / 2, surface_height_ / 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, uv_plane);
    } else {
        const uint32_t uv_height = desc.height / 2;
        std::vector<uint8_t> uv_plane_contiguous = CopyPlaneContiguous(uv_plane, desc.width, uv_height, desc.stride);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface_width_ / 2, surface_height_ / 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, uv_plane_contiguous.data());
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_y_);
    glUniform1i(uni_y_tex_, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_uv_);
    glUniform1i(uni_uv_tex_, 1);
    glUniform1i(uni_is_nv21_, config_.input_format == PixelFormat::kNV21 ? 1 : 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex_, 0);

    glEnableVertexAttribArray(static_cast<GLuint>(attr_pos_));
    glEnableVertexAttribArray(static_cast<GLuint>(attr_uv_));
    glVertexAttribPointer(static_cast<GLuint>(attr_pos_), 2, GL_FLOAT, GL_FALSE, 0, kQuadVertices);
    glVertexAttribPointer(static_cast<GLuint>(attr_uv_), 2, GL_FLOAT, GL_FALSE, 0, kQuadTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(static_cast<GLuint>(attr_pos_));
    glDisableVertexAttribArray(static_cast<GLuint>(attr_uv_));

    BufferMapping dst_mapping;
    if (dst.Map(BufferAccessMode::kWrite, &dst_mapping) != BsvError::kOk) {
        mutable_src.Unmap(&src_mapping);
        return BsvError::kInvalidArgument;
    }
    std::vector<uint8_t> readback(static_cast<size_t>(desc.width) * desc.height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, surface_width_, surface_height_, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());
    const size_t row_bytes = static_cast<size_t>(desc.width) * 4;
    auto* dst_bytes = static_cast<uint8_t*>(dst_mapping.data);
    for (uint32_t y = 0; y < desc.height; ++y) {
        const uint32_t src_row = desc.height - 1 - y;
        std::memcpy(dst_bytes + static_cast<size_t>(y) * out_desc.stride * 4,
                    readback.data() + static_cast<size_t>(src_row) * row_bytes,
                    row_bytes);
    }
    dst.Unmap(&dst_mapping);
    mutable_src.Unmap(&src_mapping);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return BsvError::kOk;
}

std::vector<uint8_t> ConvertNv12ToRgba(const IBuffer& src) {
    return ConvertNvToRgba(src, false);
}

std::vector<uint8_t> ConvertNv21ToRgba(const IBuffer& src) {
    return ConvertNvToRgba(src, true);
}

}  // namespace bsv
