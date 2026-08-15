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
              const WbmMesh* smallBoxMesh = nullptr,
              const WbmMesh* bigBoxMesh = nullptr,
              const WbmMesh* explosiveBarrelMesh = nullptr);
    void shutdown();
    void draw(const GameWorld& world);
    void drawTitleScreen(std::uint32_t selectedItem, bool optionsOpen);

private:
    static constexpr std::uint32_t MaxVertices = 32768;

    void beginGeometry();
    void appendVertex(float x, float y, float r, float g, float b, float a = 1.0f);
    void addTriangle(float ax, float ay,
                     float bx, float by,
                     float cx, float cy,
                     float r, float g, float b, float a = 1.0f);
    void addQuad(float minX, float minY,
                 float maxX, float maxY,
                 float r, float g, float b, float a = 1.0f);
    void addDiamond(float cx, float cy, float radius,
                    float r, float g, float b, float a = 1.0f);
    void addMapMesh();
    bool addPropMesh(const WbmMesh* mesh, const Vec3& position, float worldRadius);
    void addCarHazard(const GameWorld& world);
    void addSpawnedProps(const GameWorld& world);
    void addTitleGeometry(std::uint32_t selectedItem, bool optionsOpen);
    void uploadGeometry();
    void drawCurrentGeometry();
    void renderGeometry(float clearR, float clearG, float clearB);

    float worldToClipX(float x) const;
    float worldToClipY(float z) const;
    float mapToClipX(float x) const;
    float mapToClipY(float z) const;

    WHBGfxShaderGroup m_shader{};
    GX2RBuffer m_positionBuffer{};
    GX2RBuffer m_colourBuffer{};
    std::vector<float> m_positions;
    std::vector<float> m_colours;
    const WbmMesh* m_mapMesh = nullptr;
    const WbmMesh* m_smallBoxMesh = nullptr;
    const WbmMesh* m_bigBoxMesh = nullptr;
    const WbmMesh* m_explosiveBarrelMesh = nullptr;
    std::uint32_t m_vertexCount = 0;
    bool m_ready = false;
};

} // namespace webeast::wiiu
