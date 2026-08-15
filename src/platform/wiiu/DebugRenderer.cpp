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

    const GX2RResourceFlags flags = GX2R_RESOURCE_BIND_VERTEX_BUFFER |
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
        GX2RDestroyBufferEx(&m_positionBuffer, GX2R_RESOURCE_BIND_NONE);
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
    GX2RDestroyBufferEx(&m_positionBuffer, GX2R_RESOURCE_BIND_NONE);
    GX2RDestroyBufferEx(&m_colourBuffer, GX2R_RESOURCE_BIND_NONE);
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

        // In the top-down prototype renderer, only upward-facing surfaces are useful.
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

void DebugRenderer::addCarHazard(const GameWorld& world) {
    const CarHazardState& car = world.car();

    if (car.phase == CarHazardPhase::Warning) {
        // Visual lane warning accompanies the horn SFX.
        const float laneY = worldToClipY(car.position.z);
        addQuad(-0.90f, laneY - 0.018f, 0.90f, laneY + 0.018f,
                1.0f, 0.12f, 0.08f, 1.0f);
        return;
    }

    if (car.phase != CarHazardPhase::Driving) return;

    const float x = worldToClipX(car.position.x);
    const float y = worldToClipY(car.position.z);
    const float halfW = 0.12f;
    const float halfH = 0.065f;

    // Temporary car marker. The Map 2 vehicle mesh can replace this without
    // touching the hazard timing/collision code.
    addQuad(x - halfW, y - halfH, x + halfW, y + halfH,
            0.12f, 0.12f, 0.15f, 1.0f);
    addQuad(x - halfW * 0.60f, y - halfH * 0.55f,
            x + halfW * 0.60f, y + halfH * 0.55f,
            0.30f, 0.68f, 0.90f, 1.0f);
}

void DebugRenderer::addSpawnedProps(const GameWorld& world) {
    for (std::size_t i = 0; i < world.propCount(); ++i) {
        const SpawnedMapProp& prop = world.prop(i);
        if (!prop.active) continue;

        const float x = worldToClipX(prop.position.x);
        const float y = worldToClipY(prop.position.z);

        switch (prop.type) {
        case MapPropType::SmallBox:
            addQuad(x - 0.030f, y - 0.030f, x + 0.030f, y + 0.030f,
                    0.72f, 0.48f, 0.20f, 1.0f);
            break;
        case MapPropType::BigBox:
            addQuad(x - 0.050f, y - 0.050f, x + 0.050f, y + 0.050f,
                    0.48f, 0.28f, 0.12f, 1.0f);
            break;
        case MapPropType::ExplosiveBarrel:
            addDiamond(x, y, 0.052f, 0.95f, 0.18f, 0.08f, 1.0f);
            addDiamond(x, y, 0.026f, 1.0f, 0.72f, 0.10f, 1.0f);
            break;
        case MapPropType::None:
            break;
        }
    }
}

void DebugRenderer::addTitleGeometry(std::uint32_t selectedItem, bool optionsOpen) {
    // The geometry version mirrors the colours/layout of the supplied title
    // assets. It gives us a fully navigable title screen before the PNG->GX2
    // texture uploader lands.

    // Halftone corner from title background.
    for (int row = 0; row < 6; ++row) {
        for (int column = 0; column < 8 - row; ++column) {
            const float x = -0.96f + static_cast<float>(column) * 0.055f;
            const float y = 0.92f - static_cast<float>(row) * 0.065f;
            addDiamond(x, y, 0.012f, 0.02f, 0.02f, 0.03f, 1.0f);
        }
    }

    // Stylised WE BEAST logo block: cyan body, white highlight and dark shadow.
    addQuad(-0.52f, 0.31f, 0.52f, 0.69f, 0.00f, 0.05f, 0.06f, 1.0f);
    addQuad(-0.45f, 0.36f, 0.45f, 0.64f, 0.02f, 0.70f, 0.78f, 1.0f);
    addQuad(-0.35f, 0.43f, 0.35f, 0.57f, 0.96f, 0.98f, 0.98f, 1.0f);

    const bool playSelected = selectedItem == 0;
    const bool optionsSelected = selectedItem == 1;

    // PLAY button: gold normally, blue/lilac when selected, matching the
    // supplied Play.png and Play presed.png states.
    if (playSelected) {
        addQuad(-0.39f, -0.06f, 0.39f, 0.12f, 0.55f, 0.65f, 0.93f, 1.0f);
        addQuad(-0.35f, -0.025f, 0.35f, 0.085f, 0.68f, 0.74f, 0.98f, 1.0f);
    } else {
        addQuad(-0.39f, -0.06f, 0.39f, 0.12f, 0.84f, 0.67f, 0.18f, 1.0f);
        addQuad(-0.35f, -0.025f, 0.35f, 0.085f, 0.96f, 0.86f, 0.47f, 1.0f);
    }

    // Orange centre mark stands in for the PLAY lettering until texture upload.
    addQuad(-0.16f, 0.015f, 0.16f, 0.055f, 1.0f, 0.34f, 0.08f, 1.0f);

    if (optionsSelected) {
        addQuad(-0.39f, -0.34f, 0.39f, -0.16f, 0.64f, 0.70f, 0.69f, 1.0f);
        addQuad(-0.35f, -0.305f, 0.35f, -0.195f, 0.76f, 0.81f, 0.80f, 1.0f);
    } else {
        addQuad(-0.39f, -0.34f, 0.39f, -0.16f, 0.31f, 0.36f, 0.35f, 1.0f);
        addQuad(-0.35f, -0.305f, 0.35f, -0.195f, 0.43f, 0.48f, 0.47f, 1.0f);
    }
    addQuad(-0.18f, -0.265f, 0.18f, -0.225f, 0.97f, 0.97f, 0.97f, 1.0f);

    if (optionsOpen) {
        // Simple options overlay for now. B closes it; future settings can be
        // inserted here without changing title navigation.
        addQuad(-0.62f, -0.62f, 0.62f, 0.62f, 0.05f, 0.05f, 0.08f, 1.0f);
        addQuad(-0.54f, 0.28f, 0.54f, 0.43f, 0.24f, 0.27f, 0.29f, 1.0f);
        addQuad(-0.54f, 0.02f, 0.54f, 0.17f, 0.24f, 0.27f, 0.29f, 1.0f);
        addQuad(-0.54f, -0.24f, 0.54f, -0.09f, 0.24f, 0.27f, 0.29f, 1.0f);
    }
}

void DebugRenderer::uploadGeometry() {
    if (m_vertexCount == 0) return;
    const std::size_t byteCount = static_cast<std::size_t>(m_vertexCount) * sizeof(float) * 4;

    void* positionData = GX2RLockBufferEx(&m_positionBuffer, GX2R_RESOURCE_BIND_NONE);
    if (positionData) {
        std::memcpy(positionData, m_positions.data(), byteCount);
        GX2RUnlockBufferEx(&m_positionBuffer, GX2R_RESOURCE_BIND_NONE);
    }

    void* colourData = GX2RLockBufferEx(&m_colourBuffer, GX2R_RESOURCE_BIND_NONE);
    if (colourData) {
        std::memcpy(colourData, m_colours.data(), byteCount);
        GX2RUnlockBufferEx(&m_colourBuffer, GX2R_RESOURCE_BIND_NONE);
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

void DebugRenderer::renderGeometry(float clearR, float clearG, float clearB) {
    uploadGeometry();

    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(clearR, clearG, clearB, 1.0f);
    drawCurrentGeometry();
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(clearR, clearG, clearB, 1.0f);
    drawCurrentGeometry();
    WHBGfxFinishRenderDRC();
    WHBGfxFinishRender();
}

void DebugRenderer::drawTitleScreen(std::uint32_t selectedItem, bool optionsOpen) {
    if (!m_ready) return;
    beginGeometry();
    addTitleGeometry(selectedItem, optionsOpen);
    renderGeometry(0.365f, 0.090f, 0.918f);
}

void DebugRenderer::draw(const GameWorld& world) {
    if (!m_ready) return;

    beginGeometry();
    addMapMesh();
    addCarHazard(world);
    addSpawnedProps(world);

    // Player and Ball stay as clear debug markers until their WBM meshes are
    // connected. They are drawn last so they remain readable over the map.
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

    renderGeometry(0.035f, 0.040f, 0.055f);
}

} // namespace webeast::wiiu
