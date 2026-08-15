#include "assets/WbmMesh.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
void u32le(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 24));
}
void f32le(std::vector<std::uint8_t>& out, float f) {
    std::uint32_t v = 0;
    std::memcpy(&v, &f, 4);
    u32le(out, v);
}
}

int main() {
    std::vector<std::uint8_t> data{'W','B','M','1'};
    u32le(data, 1); // version
    u32le(data, 3); // vertices
    u32le(data, 3); // indices
    u32le(data, 3); // flags
    for (float f : {0.f,0.f,0.f, 1.f,1.f,1.f}) f32le(data, f);

    const float xyz[3][3] = {{0,0,0},{1,0,0},{0,0,1}};
    for (int i = 0; i < 3; ++i) {
        f32le(data, xyz[i][0]); f32le(data, xyz[i][1]); f32le(data, xyz[i][2]); f32le(data, 1.f);
        data.push_back(255); data.push_back(i == 1 ? 0 : 255); data.push_back(255); data.push_back(255);
        f32le(data, i == 1 ? 1.f : 0.f); f32le(data, i == 2 ? 1.f : 0.f);
    }
    u32le(data, 0); u32le(data, 1); u32le(data, 2);

    webeast::WbmMesh mesh;
    assert(mesh.loadFromMemory(data.data(), data.size()));
    assert(mesh.vertices().size() == 3);
    assert(mesh.indices().size() == 3);
    assert(mesh.vertices()[1].x == 1.0f);
    assert(mesh.vertices()[2].z == 1.0f);
    assert(mesh.indices()[2] == 2);

    data[0] = 'X';
    assert(!mesh.loadFromMemory(data.data(), data.size()));

    std::cout << "wbm_mesh_sim: OK\n";
    return 0;
}
