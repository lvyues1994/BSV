#ifndef BSV_GPU_CSC_H
#define BSV_GPU_CSC_H

#include <cstdint>
#include <vector>

#include "bsv/core_types.h"

namespace bsv {

class LinuxGpuCscConverter final : public ICscConverter {
public:
    LinuxGpuCscConverter();
    ~LinuxGpuCscConverter() override;

    BsvError Initialize(const CscConfig& config) override;
    BsvError Start() override;
    BsvError Stop() override;
    void Shutdown() override;
    BsvError ConvertFrame(const IBuffer& src, IBuffer& dst) override;

private:
    BsvError CreateEglContext();
    void DestroyEglContext();
    BsvError CreateShaders();
    void DestroyShaders();
    BsvError ConfigureTextures(const IBuffer& src);
    BsvError DrawAndReadback(const IBuffer& src, IBuffer& dst);

    CscConfig config_{};
    bool initialized_ = false;

    void* egl_display_ = nullptr;
    void* egl_context_ = nullptr;
    void* egl_surface_ = nullptr;
    void* egl_config_ = nullptr;

    unsigned int program_ = 0;
    int attr_pos_ = -1;
    int attr_uv_ = -1;
    int uni_y_tex_ = -1;
    int uni_uv_tex_ = -1;
    int uni_is_nv21_ = -1;
    unsigned int tex_y_ = 0;
    unsigned int tex_uv_ = 0;
    unsigned int fbo_ = 0;
    unsigned int color_tex_ = 0;

    int surface_width_ = 0;
    int surface_height_ = 0;
};

class AndroidGpuCscConverter final : public ICscConverter {
public:
    AndroidGpuCscConverter();
    ~AndroidGpuCscConverter() override;

    BsvError Initialize(const CscConfig& config) override;
    BsvError Start() override;
    BsvError Stop() override;
    void Shutdown() override;
    BsvError ConvertFrame(const IBuffer& src, IBuffer& dst) override;

private:
    BsvError CreateEglContext();
    void DestroyEglContext();
    BsvError CreateShaders();
    void DestroyShaders();
    BsvError ConfigureTextures(uint32_t width, uint32_t height);
    BsvError EnsureOutputTarget(IBuffer& dst);
    BsvError UploadInput(const IBuffer& src);
    BsvError DrawToOutput(const IBuffer& src, IBuffer& dst);

    CscConfig config_{};
    bool initialized_ = false;
    int gles_version_ = 2;

    void* egl_display_ = nullptr;
    void* egl_context_ = nullptr;
    void* egl_surface_ = nullptr;
    void* egl_config_ = nullptr;

    void* egl_image_ = nullptr;
    void* output_handle_ = nullptr;

    unsigned int program_ = 0;
    int attr_pos_ = -1;
    int attr_uv_ = -1;
    int uni_y_tex_ = -1;
    int uni_uv_tex_ = -1;
    int uni_is_nv21_ = -1;
    unsigned int tex_y_ = 0;
    unsigned int tex_uv_ = 0;
    unsigned int fbo_ = 0;
    unsigned int output_tex_ = 0;

    int surface_width_ = 0;
    int surface_height_ = 0;
};

std::vector<uint8_t> ConvertNv12ToRgba(const IBuffer& src);
std::vector<uint8_t> ConvertNv21ToRgba(const IBuffer& src);

}  // namespace bsv

#endif  // BSV_GPU_CSC_H
