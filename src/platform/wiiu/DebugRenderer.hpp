#pragma once

#include "game/GameWorld.hpp"

#include <gx2r/buffer.h>
#include <whb/gfx.h>

#include <cstdint>

namespace webeast::wiiu {

class DebugRenderer {
public:
    bool init(const char* shaderPath);
    void shutdown();
    void draw(const GameWorld& world);

private:
    static constexpr std::uint32_t MaxVertices = 192;

    void beginGeometry();
    void addTriangle(float ax, float ay,
                     float bx, float by,
                     float cx, float cy,
                     float r, float g, float b, float a = 1.0f);
    void addQuad(float minX, float minY,
                 float maxX, float maxY,
                 float r, float g, float b, float a = 1.0f);
    void addDiamond(float cx, float cy, float radius,
                    float r, float g, float b, float a = 1.0f);
    void uploadGeometry();
    void drawCurrentGeometry();

    float worldToClipX(float x) const;
    float worldToClipY(float z) const;

    WHBGfxShaderGroup m_shader{};
    GX2RBuffer m_positionBuffer{};
    GX2RBuffer m_colourBuffer{};
    float m_positions[MaxVertices * 4]{};
    float m_colours[MaxVertices * 4]{};
    std::uint32_t m_vertexCount = 0;
    bool m_ready = false;
};

} // namespace webeast::wiiu
