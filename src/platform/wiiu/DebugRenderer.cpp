#include "DebugRenderer.hpp"

#include <gx2/draw.h>
#include <gx2/shaders.h>
#include <gx2r/draw.h>
#include <whb/file.h>

#include <algorithm>
#include <cstring>

namespace webeast::wiiu {

bool DebugRenderer::init(const char* shaderPath) {
    if (!shaderPath) return false;

    char* shaderData = WHBReadWholeFile(shaderPath, nullptr);
    if (!shaderData) return false;

    const bool loaded = WHBGfxLoadGFDShaderGroup(&m_shader, 0, shaderData);
    WHBFreeWholeFile(shaderData);
    if (!loaded) return false;

    if (!WHBGfxInitShaderAttribute(&m_shader,
                                   "aPosition",
                                   0,
                                   0,
                                   GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
        WHBGfxFreeShaderGroup(&m_shader);
        return false;
    }

    if (!WHBGfxInitShaderAttribute(&m_shader,
                                   "aColour",
                                   1,
                                   0,
                                   GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
        WHBGfxFreeShaderGroup(&m_shader);
        return false;
    }

    if (!WHBGfxInitFetchShader(&m_shader)) {
        WHBGfxFreeShaderGroup(&m_shader);
        return false;
    }

    const std::uint32_t flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER |
                                GX2R_RESOURCE_USAGE_CPU_READ |
                                GX2R_RESOURCE_USAGE_CPU_WRITE |
                                GX2R_RESOURCE_USAGE_GPU_READ;

    m_positionBuffer.flags = flags;
    m_positionBuffer.elemSize = sizeof(float) * 4;
    m_positionBuffer.elemCount = MaxVertices;

    m_colourBuffer.flags = flags;
    m_colourBuffer.elemSize = sizeof(float) * 4;
    m_colourBuffer.elemCount = MaxVertices;

    if (!GX2RCreateBuffer(&m_positionBuffer)) {
        WHBGfxFreeShaderGroup(&m_shader);
        return false;
    }

    if (!GX2RCreateBuffer(&m_colourBuffer)) {
        GX2RDestroyBufferEx(&m_positionBuffer, 0);
        WHBGfxFreeShaderGroup(&m_shader);
        return false;
    }

    m_ready = true;
    return true;
}

void DebugRenderer::shutdown() {
    if (!m_ready) return;

    GX2RDestroyBufferEx(&m_positionBuffer, 0);
    GX2RDestroyBufferEx(&m_colourBuffer, 0);
    WHBGfxFreeShaderGroup(&m_shader);
    m_ready = false;
}

float DebugRenderer::worldToClipX(float x) const {
    return std::clamp(x / 8.0f, -0.95f, 0.95f);
}

float DebugRenderer::worldToClipY(float z) const {
    return std::clamp(-z / 8.0f, -0.95f, 0.95f);
}

void DebugRenderer::beginGeometry() {
    m_vertexCount = 0;
}

void DebugRenderer::addTriangle(float ax, float ay,
                                float bx, float by,
                                float cx, float cy,
                                float r, float g, float b, float a) {
    if (m_vertexCount + 3 > MaxVertices) return;

    const float positions[12] = {
        ax, ay, 0.0f, 1.0f,
        bx, by, 0.0f, 1.0f,
        cx, cy, 0.0f, 1.0f,
    };

    std::memcpy(&m_positions[m_vertexCount * 4], positions, sizeof(positions));

    for (std::uint32_t i = 0; i < 3; ++i) {
        float* colour = &m_colours[(m_vertexCount + i) * 4];
        colour[0] = r;
        colour[1] = g;
        colour[2] = b;
        colour[3] = a;
    }

    m_vertexCount += 3;
}

void DebugRenderer::addQuad(float minX, float minY,
                            float maxX, float maxY,
                            float r, float g, float b, float a) {
    addTriangle(minX, minY, maxX, minY, maxX, maxY, r, g, b, a);
    addTriangle(minX, minY, maxX, maxY, minX, maxY, r, g, b, a);
}

void DebugRenderer::addDiamond(float cx, float cy, float radius,
                               float r, float g, float b, float a) {
    addTriangle(cx, cy + radius,
                cx + radius, cy,
                cx, cy - radius,
                r, g, b, a);
    addTriangle(cx, cy + radius,
                cx, cy - radius,
                cx - radius, cy,
                r, g, b, a);
}

void DebugRenderer::uploadGeometry() {
    if (m_vertexCount == 0) return;

    const std::size_t byteCount = static_cast<std::size_t>(m_vertexCount) * sizeof(float) * 4;

    void* positionData = GX2RLockBufferEx(&m_positionBuffer, 0);
    if (positionData) {
        std::memcpy(positionData, m_positions, byteCount);
        GX2RUnlockBufferEx(&m_positionBuffer, 0);
    }

    void* colourData = GX2RLockBufferEx(&m_colourBuffer, 0);
    if (colourData) {
        std::memcpy(colourData, m_colours, byteCount);
        GX2RUnlockBufferEx(&m_colourBuffer, 0);
    }
}

void DebugRenderer::drawCurrentGeometry() {
    if (m_vertexCount == 0) return;

    GX2SetFetchShader(&m_shader.fetchShader);
    GX2SetVertexShader(m_shader.vertexShader);
    GX2SetPixelShader(m_shader.pixelShader);
    GX2RSetAttributeBuffer(&m_positionBuffer, 0, m_positionBuffer.elemSize, 0);
    GX2RSetAttributeBuffer(&m_colourBuffer, 1, m_colourBuffer.elemSize, 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, m_vertexCount, 0, 1);
}

void DebugRenderer::draw(const GameWorld& world) {
    if (!m_ready) return;

    beginGeometry();

    // Simplified top-down representation of Map 1. This is deliberately
    // procedural: it validates GX2 + gameplay transforms before GLB rendering.
    addQuad(-0.82f, -0.82f, 0.82f, 0.82f,
            0.23f, 0.47f, 0.28f, 1.0f);

    // Border of the playable support area.
    constexpr float border = 0.018f;
    addQuad(-0.82f, 0.82f - border, 0.82f, 0.82f,
            0.92f, 0.75f, 0.24f, 1.0f);
    addQuad(-0.82f, -0.82f, 0.82f, -0.82f + border,
            0.92f, 0.75f, 0.24f, 1.0f);
    addQuad(-0.82f, -0.82f, -0.82f + border, 0.82f,
            0.92f, 0.75f, 0.24f, 1.0f);
    addQuad(0.82f - border, -0.82f, 0.82f, 0.82f,
            0.92f, 0.75f, 0.24f, 1.0f);

    for (std::size_t i = 0; i < world.playerCount(); ++i) {
        const PlayerState& player = world.player(i);
        const float x = worldToClipX(player.position.x);
        const float y = worldToClipY(player.position.z);
        const float size = 0.055f + std::clamp(player.position.y, 0.0f, 3.0f) * 0.006f;

        if (player.eliminated) {
            addQuad(x - size, y - size, x + size, y + size,
                    0.25f, 0.25f, 0.25f, 1.0f);
        } else {
            addQuad(x - size, y - size, x + size, y + size,
                    0.25f, 0.60f, 1.0f, 1.0f);
        }
    }

    const Vec3& ballPosition = world.ball().position();
    const float ballX = worldToClipX(ballPosition.x);
    const float ballY = worldToClipY(ballPosition.z);
    const float ballSize = 0.050f + std::clamp(ballPosition.y, 0.0f, 6.0f) * 0.004f;
    addDiamond(ballX, ballY, ballSize,
               1.0f, 0.28f, 0.16f, 1.0f);

    uploadGeometry();

    WHBGfxBeginRender();

    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    drawCurrentGeometry();
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    drawCurrentGeometry();
    WHBGfxFinishRenderDRC();

    WHBGfxFinishRender();
}

} // namespace webeast::wiiu
