#include "bsv/gpu_csc.h"

#include <cassert>

int main() {
    bsv::LinuxGpuCscConverter converter;
    bsv::CscConfig config;
    config.input_format = bsv::PixelFormat::kNV12;
    config.output_format = bsv::PixelFormat::kRGBA8888;

    assert(converter.Initialize(config) == bsv::BsvError::kOk);
    assert(converter.Start() == bsv::BsvError::kOk);
    converter.Shutdown();

    bsv::LinuxGpuCscConverter bad_converter;
    bsv::CscConfig bad_config;
    bad_config.input_format = bsv::PixelFormat::kRGBA8888;
    bad_config.output_format = bsv::PixelFormat::kRGBA8888;
    assert(bad_converter.Initialize(bad_config) == bsv::BsvError::kNotSupported);
    bad_converter.Shutdown();
    return 0;
}
