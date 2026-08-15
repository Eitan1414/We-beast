#include "DebugRenderer.hpp"

#include <gx2/draw.h>
#include <gx2/shaders.h>
#include <gx2r/draw.h>
#include <whb/file.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace webeast::wiiu {

bool DebugRenderer::init(const char* shaderPath, const WbmMesh* mapMesh) {
    if (!shaderPath) return false;

    char* shaderData = WHBReadWholeFile(shaderPath, nullptr);
    if (!shaderData) return false;

    const bool loaded = WHBGfxLoadGFDShaderGroup(&m_shader, 0, shaderData);
    WHBFreeWholeFile(shaderData);
    if (!loaded) return false;

    if (!WHBGfxInitShaderAttribute(&m_shader, "aPosition", 0, 0,
                                   GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) ||
        !WHBGfxInitShaderAttribute(&m_shader, "aColour", 1, 0,
                                   GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) ||
        !WHBGfxInitFetchShader(&m_shader)) {
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

    m_positions.reserve(MaxVertices * 4);
    m_colours.reserve(MaxVertices * 4);
    m_mapMesh = mapMesh;
    m_ready = true;
    return true;
}

void DebugRenderer::shutdown() {
    if (!m_ready) return;
    GX2RDestroyBufferEx(&m_positionBuffer, 0);
    GX2RDestroyBufferEx(&m_colourBuffer, 0);
    WHBGfxFreeShaderGroup(&m_shader);
    m_positions.clear();
    m_colours.clear();
    m_mapMesh = nullptr;
    m_ready = false;
}

float DebugRenderer::worldToClipX(float x) const {
    return std::clamp((x / 6.5f) * 0.88f, -0.94f, 0.94f);
}

float DebugRenderer::worldToClipY(float z) const {
    return std::clamp((-z / 6.5f) * 0.88f, -0.94f, 0.94f);
}

float DebugRenderer::mapToClipX(float x) const {
    if (!m_mapMesh || !m_mapMesh->valid()) return 0.0f;
    const WbmBounds& b = m_mapMesh->bounds();
    const float width = b.maxX - b.minX;
    if (std::fabs(width) < 0.000001f) return 0.0f;
    return (((x - b.minX) / width) * 2.0f - 1.0f) * 0.88f;
}

float DebugRenderer::mapToClipY(float z) const {
    if (!m_mapMesh || !m_mapMesh->valid()) return 0.0f;
    const WbmBounds& b = m_mapMesh->bounds();
    const float depth = b.maxZ - b.minZ;
    if (std::fabs(depth) < 0.000001f) return 0.0f;
    return -(((z - b.minZ) / depth) * 2.0f - 1.0f) * 0.88f;
}

void DebugRenderer::beginGeometry() {
    m_vertexCount = 0;
    m_positions.clear();
    m_colours.clear();
}

void DebugRenderer::appendVertex(float x, float y,
                                 float r, float g, float b, float a) {
    if (m_vertexCount >= MaxVertices) return;
    m_positions.insert(m_positions.end(), {x, y, 0.0f, 1.0f});
    m_colours.insert(m_colours.end(), {r, g, b, a});
    ++m_vertexCount;
}

void DebugRenderer::addTriangle(float ax, float ay,
                                float bx, float by,
                                float cx, float cy,
                                float r, float g, float b, float a) {
    if (m_vertexCount + 3 > MaxVertices) return;
    appendVertex(ax, ay, r, g, b, a);
    appendVertex(bx, by, r, g, b, a);
    appendVertex(cx, cy, r, g, b, a);
}

void DebugRenderer::addQuad(float minX, float minY,
                            float maxX, float maxY,
                            float r, float g, float b, float a) {
    addTriangle(minX, minY, maxX, minY, maxX, maxY, r, g, b, a);
    addTriangle(minX, minY, maxX, maxY, minX, maxY, r, g, b, a);
}

void DebugRenderer::addDiamond(float cx, float cy, float radius,
                               float r, float g, float b, float a) {
    addTriangle(cx, cy + radius, cx + radius, cy, cx, cy - radius, r, g, b, a);
    addTriangle(cx, cy + radius, cx, cy - radius, cx - radius, cy, r, g, b, a);
}

void DebugRenderer::addMapMesh() {
    if (!m_mapMesh || !m_mapMesh->valid()) {
        addQuad(-0.88f, -0.88f, 0.88f, 0.88f, 0.23f, 0.47f, 0.28f, 1.0f);
        return;
    }

    const auto& vertices = m_mapMesh->vertices();
    const auto& indices = m_mapMesh->indices();

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        if (m_vertexCount + 3 > MaxVertices) break;

        const WbmVertex& a = vertices[indices[i]];
        const WbmVertex& b = vertices[indices[i + 1]];
        const WbmVertex& c = vertices[indices[i + 2]];

        // In the top-down V0.1 renderer, only upward-facing surfaces are useful.
        // This also prevents the carpet/box side walls from painting over its top.
        const float ux = b.x - a.x;
        const float uz = b.z - a.z;
        const float vx = c.x - a.x;
        const float vz = c.z - a.z;
        const float normalY = uz * vx - ux * vz;
        if (normalY <= 0.0000001f) continue;

        const WbmVertex* tri[3] = {&a, &b, &c};
        for (const WbmVertex* v : tri) {
            appendVertex(mapToClipX(v->x), mapToClipY(v->z),
                         v->r / 255.0f, v->g / 255.0f,
                         v->b / 255.0f, v->a / 255.0f);
        }
    }
}

void DebugRenderer::uploadGeometry() {
    if (m_vertexCount == 0) return;
    const std::size_t byteCount = static_cast<std::size_t>(m_vertexCount) * sizeof(float) * 4;

    void* positionData = GX2RLockBufferEx(&m_positionBuffer, 0);
    if (positionData) {
        std::memcpy(positionData, m_positions.data(), byteCount);
        GX2RUnlockBufferEx(&m_positionBuffer, 0);
    }

    void* colourData = GX2RLockBufferEx(&m_colourBuffer, 0);
    if (colourData) {
        std::memcpy(colourData, m_colours.data(), byteCount);
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
    addMapMesh();

    // Player and Ball stay as clear debug markers until their WBM meshes are
    // connected. They are drawn last so they remain readable over Map 1.
    for (std::size_t i = 0; i < world.playerCount(); ++i) {
        const PlayerState& player = world.player(i);
        const float x = worldToClipX(player.position.x);
        const float y = worldToClipY(player.position.z);
        const float size = 0.045f + std::clamp(player.position.y, 0.0f, 3.0f) * 0.006f;
        if (player.eliminated) {
            addQuad(x - size, y - size, x + size, y + size, 0.18f, 0.18f, 0.18f, 1.0f);
        } else {
            addQuad(x - size, y - size, x + size, y + size, 0.15f, 0.55f, 1.0f, 1.0f);
        }
    }

    const Vec3& ball = world.ball().position();
    addDiamond(worldToClipX(ball.x), worldToClipY(ball.z), 0.045f,
               1.0f, 0.18f, 0.10f, 1.0f);

    uploadGeometry();

    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.035f, 0.040f, 0.055f, 1.0f);
    drawCurrentGeometry();
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.035f, 0.040f, 0.055f, 1.0f);
    drawCurrentGeometry();
    WHBGfxFinishRenderDRC();
    WHBGfxFinishRender();
}

} // namespace webeast::wiiu
