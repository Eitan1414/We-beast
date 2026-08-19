#pragma once

#include "assets/WbmMesh.hpp"
#include "game/GameWorld.hpp"

#include <gx2r/buffer.h>
#include <whb/gfx.h>

#include <cstdint>
#include <vector>

namespace webeast::wiiu {

class DebugRenderer {
public:
    bool init(const char* shaderPath,
              const WbmMesh* mapMesh,
              const WbmMesh* dummyMesh,
              const WbmMesh* smallBoxMesh = nullptr,
              const WbmMesh* bigBoxMesh = nullptr,
              const WbmMesh* explosiveBarrelMesh = nullptr);
    void shutdown();
    void draw(const GameWorld& world);
    void drawTitleScreen(std::uint32_t selectedItem, bool optionsOpen);

private:
    // Map 1 + two complete Dummy 1 meshes fit in this shared CPU/GX2 buffer.
    // The V0.1 assets use about 135k submitted vertices at worst.
    static constexpr std::uint32_t MaxVertices = 150000;

    void beginGeometry();
    void appendVertex(float x, float y,
                      float r, float g, float b, float a = 1.0f);
    void appendVertex3D(float x, float y, float z,
                        float r, float g, float b, float a = 1.0f);
    void addTriangle(float ax, float ay,
                     float bx, float by,
                     float cx, float cy,
                     float r, float g, float b, float a = 1.0f);
    void addQuad(float minX, float minY,
                 float maxX, float maxY,
                 float r, float g, float b, float a = 1.0f);
    void addDiamond(float cx, float cy, float radius,
                    float r, float g, float b, float a = 1.0f);

    void projectWorld(float x, float y, float z,
                      float& clipX, float& clipY, float& clipZ) const;
    void addMapMesh();
    bool addPlayerMesh(const PlayerState& player,
                       const Vec3& facing,
                       float tintR, float tintG, float tintB);
    bool addPropMesh(const WbmMesh* mesh, const Vec3& position, float worldRadius);
    void addCarHazard(const GameWorld& world);
    void addSpawnedProps(const GameWorld& world);
    void addTitleGeometry(std::uint32_t selectedItem, bool optionsOpen);
    void uploadGeometry();
    void drawCurrentGeometry();
    void renderGeometry(float clearR, float clearG, float clearB);

    float worldToClipX(float x) const;
    float worldToClipY(float z) const;

    WHBGfxShaderGroup m_shader{};
    GX2RBuffer m_positionBuffer{};
    GX2RBuffer m_colourBuffer{};
    std::vector<float> m_positions;
    std::vector<float> m_colours;
    const WbmMesh* m_mapMesh = nullptr;
    const WbmMesh* m_dummyMesh = nullptr;
    const WbmMesh* m_smallBoxMesh = nullptr;
    const WbmMesh* m_bigBoxMesh = nullptr;
    const WbmMesh* m_explosiveBarrelMesh = nullptr;
    std::uint32_t m_vertexCount = 0;
    bool m_ready = false;
};

} // namespace webeast::wiiu
