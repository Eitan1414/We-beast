#include "DebugRenderer.hpp"
#include "game/GameWorld.hpp"

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <vpad/input.h>
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

bool initRenderer(webeast::wiiu::DebugRenderer& renderer, const char* sdRoot) {
    if (!sdRoot) return false;

    char shaderPath[512]{};

    // Normal We Beast SD layout.
    std::snprintf(shaderPath,
                  sizeof(shaderPath),
                  "%s/wiiu/apps/webeast/content/pos_col_shader.gsh",
                  sdRoot);
    if (renderer.init(shaderPath)) return true;

    // Development fallback matching the layout used by the official WUT GX2
    // sample. This makes early hardware tests easier while the app packaging
    // is still evolving.
    std::snprintf(shaderPath,
                  sizeof(shaderPath),
                  "%s/wut/content/pos_col_shader.gsh",
                  sdRoot);
    return renderer.init(shaderPath);
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

    webeast::wiiu::DebugRenderer renderer;
    if (!initRenderer(renderer, WHBGetSdCardMountPath())) {
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -3;
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

        // WHBGfxFinishRender synchronizes presentation, but the small sleep
        // also keeps this first harness from spinning aggressively if a video
        // mode behaves unexpectedly during development.
        OSSleepTicks(OSMillisecondsToTicks(1));
    }

    renderer.shutdown();
    WHBUnmountSdCard();
    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
