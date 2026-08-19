#include "DebugRenderer.hpp"

#include <gx2/draw.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2r/draw.h>
#include <whb/file.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace webeast::wiiu {

bool DebugRenderer::init(const char* shaderPath,
                         const WbmMesh* mapMesh,
                         const WbmMesh* dummyMesh,
                         const WbmMesh* smallBoxMesh,
                         const WbmMesh* bigBoxMesh,
                         const WbmMesh* explosiveBarrelMesh) {
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
    m_dummyMesh = dummyMesh;
    m_smallBoxMesh = smallBoxMesh;
    m_bigBoxMesh = bigBoxMesh;
    m_explosiveBarrelMesh = explosiveBarrelMesh;
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
    m_dummyMesh = nullptr;
    m_smallBoxMesh = nullptr;
    m_bigBoxMesh = nullptr;
    m_explosiveBarrelMesh = nullptr;
    m_ready = false;
}

float DebugRenderer::worldToClipX(float x) const {
    return std::clamp((x / 6.5f) * 0.88f, -0.94f, 0.94f);
}

float DebugRenderer::worldToClipY(float z) const {
    return std::clamp((-z / 6.5f) * 0.88f, -0.94f, 0.94f);
}

void DebugRenderer::projectWorld(float x, float y, float z,
                                 float& clipX, float& clipY, float& clipZ) const {
    constexpr float RightX =  0.78086881f;
    constexpr float RightY =  0.0f;
    constexpr float RightZ = -0.62469505f;
    constexpr float UpX = -0.38447322f;
    constexpr float UpY =  0.78817011f;
    constexpr float UpZ = -0.48059153f;
    constexpr float ForwardX = -0.49236596f;
    constexpr float ForwardY = -0.61545745f;
    constexpr float ForwardZ = -0.61545745f;

    const float viewX = x * RightX + y * RightY + z * RightZ;
    const float viewY = x * UpX + y * UpY + z * UpZ;
    const float viewDepth = x * ForwardX + y * ForwardY + z * ForwardZ;

    clipX = std::clamp((viewX / 8.1f) * 0.92f, -0.98f, 0.98f);
    clipY = std::clamp((viewY / 6.2f) * 0.92f, -0.98f, 0.98f);
    clipZ = std::clamp(0.50f + viewDepth / 20.0f, 0.02f, 0.98f);
}

void DebugRenderer::beginGeometry() {
    m_vertexCount = 0;
    m_positions.clear();
    m_colours.clear();
}

void DebugRenderer::appendVertex(float x, float y,
                                 float r, float g, float b, float a) {
    appendVertex3D(x, y, 0.0f, r, g, b, a);
}

void DebugRenderer::appendVertex3D(float x, float y, float z,
                                   float r, float g, float b, float a) {
    if (m_vertexCount >= MaxVertices) return;
    m_positions.insert(m_positions.end(), {x, y, z, 1.0f});
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
        addQuad(-0.78f, -0.56f, 0.78f, 0.56f, 0.23f, 0.47f, 0.28f, 1.0f);
        return;
    }

    const WbmBounds& bounds = m_mapMesh->bounds();
    const float centerX = (bounds.minX + bounds.maxX) * 0.5f;
    const float centerZ = (bounds.minZ + bounds.maxZ) * 0.5f;
    const float halfX = (bounds.maxX - bounds.minX) * 0.5f;
    const float halfZ = (bounds.maxZ - bounds.minZ) * 0.5f;
    const float sourceRadius = std::max(halfX, halfZ);
    if (sourceRadius <= 0.000001f) return;

    const float scale = 5.45f / sourceRadius;
    const auto& vertices = m_mapMesh->vertices();
    const auto& indices = m_mapMesh->indices();

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        if (m_vertexCount + 3 > MaxVertices) break;
        const WbmVertex* tri[3] = {
            &vertices[indices[i]], &vertices[indices[i + 1]], &vertices[indices[i + 2]]
        };

        for (const WbmVertex* v : tri) {
            const float worldX = (v->x - centerX) * scale;
            const float worldY = (v->y - bounds.maxY) * scale;
            const float worldZ = (v->z - centerZ) * scale;
            float x = 0.0f, y = 0.0f, z = 0.5f;
            projectWorld(worldX, worldY, worldZ, x, y, z);
            appendVertex3D(x, y, z,
                           v->r / 255.0f, v->g / 255.0f,
                           v->b / 255.0f, v->a / 255.0f);
        }
    }
}

bool DebugRenderer::addPlayerMesh(const PlayerState& player,
                                  const Vec3& facing,
                                  float tintR, float tintG, float tintB) {
    if (!m_dummyMesh || !m_dummyMesh->valid() || player.eliminated) return false;

    const WbmBounds& bounds = m_dummyMesh->bounds();
    const float height = bounds.maxY - bounds.minY;
    if (height <= 0.000001f) return false;

    const float centerX = (bounds.minX + bounds.maxX) * 0.5f;
    const float centerZ = (bounds.minZ + bounds.maxZ) * 0.5f;
    const float scale = 1.70f / height;

    float fx = facing.x;
    float fz = facing.z;
    const float facingLength = std::sqrt(fx * fx + fz * fz);
    if (facingLength <= 0.0001f) {
        fx = 1.0f;
        fz = 0.0f;
    } else {
        fx /= facingLength;
        fz /= facingLength;
    }

    const float yaw = std::atan2(fx, fz);
    const float cosYaw = std::cos(yaw);
    const float sinYaw = std::sin(yaw);

    const auto& vertices = m_dummyMesh->vertices();
    const auto& indices = m_dummyMesh->indices();
    const std::uint32_t startVertex = m_vertexCount;

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        if (m_vertexCount + 3 > MaxVertices) break;
        const WbmVertex* tri[3] = {
            &vertices[indices[i]], &vertices[indices[i + 1]], &vertices[indices[i + 2]]
        };

        for (const WbmVertex* v : tri) {
            const float localX = (v->x - centerX) * scale;
            const float localY = (v->y - bounds.minY) * scale;
            const float localZ = (v->z - centerZ) * scale;

            const float rotatedX = localX * cosYaw + localZ * sinYaw;
            const float rotatedZ = -localX * sinYaw + localZ * cosYaw;
            const float worldX = player.position.x + rotatedX;
            const float worldY = player.position.y + localY;
            const float worldZ = player.position.z + rotatedZ;

            float x = 0.0f, y = 0.0f, z = 0.5f;
            projectWorld(worldX, worldY, worldZ, x, y, z);
            appendVertex3D(x, y, z,
                           std::clamp((v->r / 255.0f) * tintR, 0.0f, 1.0f),
                           std::clamp((v->g / 255.0f) * tintG, 0.0f, 1.0f),
                           std::clamp((v->b / 255.0f) * tintB, 0.0f, 1.0f),
                           v->a / 255.0f);
        }
    }

    return m_vertexCount > startVertex;
}

bool DebugRenderer::addPropMesh(const WbmMesh* mesh,
                                const Vec3& position,
                                float worldRadius) {
    if (!mesh || !mesh->valid() || worldRadius <= 0.0f) return false;

    const WbmBounds& bounds = mesh->bounds();
    const float centerX = (bounds.minX + bounds.maxX) * 0.5f;
    const float centerZ = (bounds.minZ + bounds.maxZ) * 0.5f;
    const float halfX = (bounds.maxX - bounds.minX) * 0.5f;
    const float halfZ = (bounds.maxZ - bounds.minZ) * 0.5f;
    const float sourceRadius = std::max(halfX, halfZ);
    if (sourceRadius <= 0.000001f) return false;

    const float scale = worldRadius / sourceRadius;
    const auto& vertices = mesh->vertices();
    const auto& indices = mesh->indices();
    const std::uint32_t startVertex = m_vertexCount;

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        if (m_vertexCount + 3 > MaxVertices) break;
        const WbmVertex* tri[3] = {
            &vertices[indices[i]], &vertices[indices[i + 1]], &vertices[indices[i + 2]]
        };
        for (const WbmVertex* v : tri) {
            const float worldX = position.x + (v->x - centerX) * scale;
            const float worldY = position.y + (v->y - bounds.minY) * scale;
            const float worldZ = position.z + (v->z - centerZ) * scale;
            float x = 0.0f, y = 0.0f, z = 0.5f;
            projectWorld(worldX, worldY, worldZ, x, y, z);
            appendVertex3D(x, y, z,
                           v->r / 255.0f, v->g / 255.0f,
                           v->b / 255.0f, v->a / 255.0f);
        }
    }

    return m_vertexCount > startVertex;
}

void DebugRenderer::addCarHazard(const GameWorld& world) {
    const CarHazardState& car = world.car();
    if (car.phase == CarHazardPhase::Waiting) return;

    if (car.phase == CarHazardPhase::Warning) {
        const float laneY = worldToClipY(car.position.z);
        addQuad(-0.90f, laneY - 0.018f, 0.90f, laneY + 0.018f,
                1.0f, 0.12f, 0.08f, 1.0f);
        return;
    }

    const float x = worldToClipX(car.position.x);
    const float y = worldToClipY(car.position.z);
    addQuad(x - 0.12f, y - 0.065f, x + 0.12f, y + 0.065f,
            0.12f, 0.12f, 0.15f, 1.0f);
}

void DebugRenderer::addSpawnedProps(const GameWorld& world) {
    for (std::size_t i = 0; i < world.propCount(); ++i) {
        const SpawnedMapProp& prop = world.prop(i);
        if (!prop.active) continue;

        bool renderedMesh = false;
        switch (prop.type) {
        case MapPropType::SmallBox:
            renderedMesh = addPropMesh(m_smallBoxMesh, prop.position, 0.26f);
            break;
        case MapPropType::BigBox:
            renderedMesh = addPropMesh(m_bigBoxMesh, prop.position, 0.42f);
            break;
        case MapPropType::ExplosiveBarrel:
            renderedMesh = addPropMesh(m_explosiveBarrelMesh, prop.position, 0.32f);
            break;
        case MapPropType::None:
            break;
        }
        if (renderedMesh) continue;

        const float x = worldToClipX(prop.position.x);
        const float y = worldToClipY(prop.position.z);
        addDiamond(x, y, 0.04f, 0.80f, 0.46f, 0.12f, 1.0f);
    }
}

void DebugRenderer::addTitleGeometry(std::uint32_t selectedItem, bool optionsOpen) {
    for (int row = 0; row < 6; ++row) {
        for (int column = 0; column < 8 - row; ++column) {
            const float x = -0.96f + static_cast<float>(column) * 0.055f;
            const float y = 0.92f - static_cast<float>(row) * 0.065f;
            addDiamond(x, y, 0.012f, 0.02f, 0.02f, 0.03f, 1.0f);
        }
    }

    addQuad(-0.52f, 0.31f, 0.52f, 0.69f, 0.00f, 0.05f, 0.06f, 1.0f);
    addQuad(-0.45f, 0.36f, 0.45f, 0.64f, 0.02f, 0.70f, 0.78f, 1.0f);
    addQuad(-0.35f, 0.43f, 0.35f, 0.57f, 0.96f, 0.98f, 0.98f, 1.0f);

    const bool playSelected = selectedItem == 0;
    const bool optionsSelected = selectedItem == 1;

    if (playSelected) {
        addQuad(-0.39f, -0.06f, 0.39f, 0.12f, 0.55f, 0.65f, 0.93f, 1.0f);
        addQuad(-0.35f, -0.025f, 0.35f, 0.085f, 0.68f, 0.74f, 0.98f, 1.0f);
    } else {
        addQuad(-0.39f, -0.06f, 0.39f, 0.12f, 0.84f, 0.67f, 0.18f, 1.0f);
        addQuad(-0.35f, -0.025f, 0.35f, 0.085f, 0.96f, 0.86f, 0.47f, 1.0f);
    }

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
    GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_LEQUAL);
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

    for (std::size_t i = 0; i < world.playerCount(); ++i) {
        const PlayerState& player = world.player(i);
        if (player.eliminated) continue;

        float tintR = 1.0f;
        float tintG = 1.0f;
        float tintB = 1.0f;
        if (world.isGrabbed(i)) {
            tintR = 1.0f;
            tintG = 0.86f;
            tintB = 0.38f;
        }

        Vec3 visualFacing{player.velocity.x, 0.0f, player.velocity.z};
        if (std::fabs(visualFacing.x) + std::fabs(visualFacing.z) < 0.05f) {
            visualFacing = world.isTrainingDummy(i) ? Vec3{-1.0f, 0.0f, 0.0f}
                                                    : Vec3{ 1.0f, 0.0f, 0.0f};
        }

        if (!addPlayerMesh(player, visualFacing, tintR, tintG, tintB)) {
            const float x = worldToClipX(player.position.x);
            const float y = worldToClipY(player.position.z);
            const float size = 0.060f;
            addQuad(x - size, y - size, x + size, y + size,
                    world.isTrainingDummy(i) ? 1.0f : 0.02f,
                    world.isTrainingDummy(i) ? 0.06f : 0.55f,
                    world.isTrainingDummy(i) ? 0.04f : 1.0f,
                    1.0f);
        }
    }

    if (world.ballEnabled()) {
        const Vec3& ball = world.ball().position();
        addDiamond(worldToClipX(ball.x), worldToClipY(ball.z), 0.045f,
                   1.0f, 0.18f, 0.10f, 1.0f);
    }

    renderGeometry(0.035f, 0.040f, 0.055f);
}

} // namespace webeast::wiiu
