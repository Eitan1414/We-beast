#include "WbmMesh.hpp"

#include <cstring>
#include <limits>

namespace webeast {
namespace {

constexpr std::size_t HeaderSize = 44;
constexpr std::size_t VertexSize = 28;
constexpr std::uint32_t SupportedVersion = 1;
constexpr std::uint32_t MaxVertices = 1u << 20;
constexpr std::uint32_t MaxIndices = 1u << 22;

std::uint32_t readU32LE(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

float readF32LE(const std::uint8_t* p) {
    const std::uint32_t bits = readU32LE(p);
    float value = 0.0f;
    static_assert(sizeof(value) == sizeof(bits), "WBM1 requires IEEE-754 float32");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool safeAddMultiply(std::size_t base,
                     std::uint32_t count,
                     std::size_t stride,
                     std::size_t& out) {
    if (count > (std::numeric_limits<std::size_t>::max() - base) / stride) {
        return false;
    }
    out = base + static_cast<std::size_t>(count) * stride;
    return true;
}

} // namespace

void WbmMesh::clear() {
    m_flags = 0;
    m_bounds = {};
    m_vertices.clear();
    m_indices.clear();
}

bool WbmMesh::loadFromMemory(const void* data, std::size_t size) {
    clear();
    if (!data || size < HeaderSize) return false;

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (bytes[0] != 'W' || bytes[1] != 'B' || bytes[2] != 'M' || bytes[3] != '1') {
        return false;
    }

    const std::uint32_t version = readU32LE(bytes + 4);
    const std::uint32_t vertexCount = readU32LE(bytes + 8);
    const std::uint32_t indexCount = readU32LE(bytes + 12);
    m_flags = readU32LE(bytes + 16);

    if (version != SupportedVersion || vertexCount == 0 || indexCount == 0) {
        clear();
        return false;
    }
    if (vertexCount > MaxVertices || indexCount > MaxIndices || (indexCount % 3) != 0) {
        clear();
        return false;
    }

    m_bounds.minX = readF32LE(bytes + 20);
    m_bounds.minY = readF32LE(bytes + 24);
    m_bounds.minZ = readF32LE(bytes + 28);
    m_bounds.maxX = readF32LE(bytes + 32);
    m_bounds.maxY = readF32LE(bytes + 36);
    m_bounds.maxZ = readF32LE(bytes + 40);

    std::size_t indexOffset = 0;
    if (!safeAddMultiply(HeaderSize, vertexCount, VertexSize, indexOffset)) {
        clear();
        return false;
    }

    std::size_t expectedSize = 0;
    if (!safeAddMultiply(indexOffset, indexCount, sizeof(std::uint32_t), expectedSize) ||
        expectedSize > size) {
        clear();
        return false;
    }

    m_vertices.resize(vertexCount);
    const std::uint8_t* vertexBytes = bytes + HeaderSize;
    for (std::uint32_t i = 0; i < vertexCount; ++i) {
        const std::uint8_t* p = vertexBytes + static_cast<std::size_t>(i) * VertexSize;
        WbmVertex& v = m_vertices[i];
        v.x = readF32LE(p + 0);
        v.y = readF32LE(p + 4);
        v.z = readF32LE(p + 8);
        v.w = readF32LE(p + 12);
        v.r = p[16];
        v.g = p[17];
        v.b = p[18];
        v.a = p[19];
        v.u = readF32LE(p + 20);
        v.v = readF32LE(p + 24);
    }

    m_indices.resize(indexCount);
    const std::uint8_t* indexBytes = bytes + indexOffset;
    for (std::uint32_t i = 0; i < indexCount; ++i) {
        const std::uint32_t index = readU32LE(indexBytes + static_cast<std::size_t>(i) * 4);
        if (index >= vertexCount) {
            clear();
            return false;
        }
        m_indices[i] = index;
    }

    return true;
}

} // namespace webeast
