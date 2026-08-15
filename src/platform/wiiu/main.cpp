#include "game/GameWorld.hpp"

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <vpad/input.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>

#include <cmath>

using namespace webeast;

namespace {

float applyDeadzone(float v, float deadzone = 0.14f) {
    if (std::fabs(v) <= deadzone) return 0.0f;
    const float sign = v < 0.0f ? -1.0f : 1.0f;
    return sign * ((std::fabs(v) - deadzone) / (1.0f - deadzone));
}

} // namespace

int main(int, char**) {
    WHBProcInit();
    WHBLogConsoleInit();
    WHBLogPrintf("We Beast V0.1 - Wii U gameplay harness");
    WHBLogPrintf("Left stick: move | A: jump | -: reset");

    GameWorldConfig config{};
    GameWorld world(0x57424541u);
    world.reset(1, config);

    int logCounter = 0;

    while (WHBProcIsRunning()) {
        PlayerInput input{};

        VPADStatus status{};
        VPADReadError error = VPAD_READ_SUCCESS;
        if (VPADRead(VPAD_CHAN_0, &status, 1, &error) > 0) {
            input.moveX = applyDeadzone(status.leftStick.x);
            input.moveZ = -applyDeadzone(status.leftStick.y);
            input.jumpPressed = (status.trigger & VPAD_BUTTON_A) != 0;

            if ((status.trigger & VPAD_BUTTON_MINUS) != 0) {
                world.reset(1, config);
                WHBLogPrintf("Round reset");
            }
        }

        world.update(1.0f / 60.0f, &input, 1);

        if (++logCounter >= 30) {
            logCounter = 0;
            const PlayerState& p = world.player(0);
            const Vec3& b = world.ball().position();
            WHBLogPrintf("P %.2f %.2f %.2f | Ball %.2f %.2f %.2f | %s",
                         p.position.x, p.position.y, p.position.z,
                         b.x, b.y, b.z,
                         p.eliminated ? "OUT" : "ALIVE");
        }

        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    WHBLogConsoleFree();
    WHBProcShutdown();
    return 0;
}
