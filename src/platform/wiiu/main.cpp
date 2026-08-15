#include "DebugRenderer.hpp"
#include "assets/WbmMesh.hpp"
#include "game/GameWorld.hpp"

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <vpad/input.h>
#include <whb/file.h>
#include <whb/gfx.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#include <cmath>
#include <cstdio>

using namespace webeast;

namespace {

float applyDeadzone(float v, float deadzone = 0.14f) {
    if (std::fabs(v) <= deadzone) return 0.0f;
    const float sign = v < 0.0f ? -1.0f : 1.0f;
    return sign * ((std::fabs(v) - deadzone) / (1.0f - deadzone));
}

bool makeContentPath(char* out, std::size_t outSize,
                     const char* sdRoot, const char* fileName,
                     bool fallback) {
    if (!out || !sdRoot || !fileName) return false;
    const int written = fallback
        ? std::snprintf(out, outSize, "%s/wut/content/%s", sdRoot, fileName)
        : std::snprintf(out, outSize, "%s/wiiu/apps/webeast/content/%s", sdRoot, fileName);
    return written > 0 && static_cast<std::size_t>(written) < outSize;
}

bool loadMap(WbmMesh& mesh, const char* sdRoot) {
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeContentPath(path, sizeof(path), sdRoot, "map_01.wbm", fallback != 0)) {
            continue;
        }
        std::uint32_t size = 0;
        char* bytes = WHBReadWholeFile(path, &size);
        if (!bytes) continue;
        const bool ok = mesh.loadFromMemory(bytes, size);
        WHBFreeWholeFile(bytes);
        if (ok) return true;
    }
    return false;
}

bool initRenderer(webeast::wiiu::DebugRenderer& renderer,
                  const WbmMesh* mapMesh,
                  const char* sdRoot) {
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeContentPath(path, sizeof(path), sdRoot,
                             "pos_col_shader.gsh", fallback != 0)) {
            continue;
        }
        if (renderer.init(path, mapMesh)) return true;
    }
    return false;
}

} // namespace

int main(int, char**) {
    WHBProcInit();

    if (!WHBGfxInit()) {
        WHBProcShutdown();
        return -1;
    }
    if (!WHBMountSdCard()) {
        WHBGfxShutdown();
        WHBProcShutdown();
        return -2;
    }

    WbmMesh mapMesh;
    if (!loadMap(mapMesh, WHBGetSdCardMountPath())) {
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -3;
    }

    webeast::wiiu::DebugRenderer renderer;
    if (!initRenderer(renderer, &mapMesh, WHBGetSdCardMountPath())) {
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -4;
    }

    GameWorldConfig config{};
    GameWorld world(0x57424541u);
    world.reset(1, config);

    while (WHBProcIsRunning()) {
        PlayerInput input{};
        VPADStatus status{};
        VPADReadError error = VPAD_READ_SUCCESS;

        if (VPADRead(VPAD_CHAN_0, &status, 1, &error) > 0 &&
            error == VPAD_READ_SUCCESS) {
            input.moveX = applyDeadzone(status.leftStick.x);
            input.moveZ = -applyDeadzone(status.leftStick.y);
            input.jumpPressed = (status.trigger & VPAD_BUTTON_A) != 0;
            if ((status.trigger & VPAD_BUTTON_MINUS) != 0) {
                world.reset(1, config);
            }
        }

        world.update(1.0f / 60.0f, &input, 1);
        renderer.draw(world);
        OSSleepTicks(OSMillisecondsToTicks(1));
    }

    renderer.shutdown();
    WHBUnmountSdCard();
    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
