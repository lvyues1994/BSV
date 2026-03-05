#ifndef BSV_GPU_CSC_SHADER_H
#define BSV_GPU_CSC_SHADER_H

#include <GLES2/gl2.h>

namespace bsv {

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

}  // namespace bsv

#endif  // BSV_GPU_CSC_SHADER_H
