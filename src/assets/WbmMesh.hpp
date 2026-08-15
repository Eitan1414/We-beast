#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace webeast {

struct WbmVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
    float u = 0.0f;
    float v = 0.0f;
};

struct WbmBounds {
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
};

class WbmMesh {
public:
    bool loadFromMemory(const void* data, std::size_t size);
    void clear();

    bool valid() const { return !m_vertices.empty() && !m_indices.empty(); }
    std::uint32_t flags() const { return m_flags; }
    const WbmBounds& bounds() const { return m_bounds; }
    const std::vector<WbmVertex>& vertices() const { return m_vertices; }
    const std::vector<std::uint32_t>& indices() const { return m_indices; }

private:
    std::uint32_t m_flags = 0;
    WbmBounds m_bounds{};
    std::vector<WbmVertex> m_vertices;
    std::vector<std::uint32_t> m_indices;
};

} // namespace webeast
