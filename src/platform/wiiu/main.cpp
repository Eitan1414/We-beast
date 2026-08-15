#include "DebugRenderer.hpp"
#include "HornPlayer.hpp"
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

bool loadMapFile(WbmMesh& mesh, const char* sdRoot, const char* fileName) {
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeContentPath(path, sizeof(path), sdRoot, fileName, fallback != 0)) {
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

int loadPreferredMap(WbmMesh& mesh, const char* sdRoot) {
    // Current development target is Map 2. Keep Map 1 as a hardware-safe
    // fallback so an older SD bundle can still boot the title screen/game.
    if (loadMapFile(mesh, sdRoot, "map_02.wbm")) return 2;
    if (loadMapFile(mesh, sdRoot, "map_01.wbm")) return 1;
    return 0;
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

bool initHorn(webeast::wiiu::HornPlayer& horn, const char* sdRoot) {
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeContentPath(path, sizeof(path), sdRoot,
                             "car_honk.pcm", fallback != 0)) {
            continue;
        }
        // The supplied MP3 is trimmed from 00:01 and preconverted offline to
        // 16 kHz mono signed 16-bit big-endian PCM for a tiny Wii U runtime path.
        if (horn.init(path, 16000)) return true;
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

    const char* sdRoot = WHBGetSdCardMountPath();

    WbmMesh mapMesh;
    const int loadedMap = loadPreferredMap(mapMesh, sdRoot);
    if (loadedMap == 0) {
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -3;
    }

    webeast::wiiu::DebugRenderer renderer;
    if (!initRenderer(renderer, &mapMesh, sdRoot)) {
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -4;
    }

    // Audio is optional at boot: a missing horn asset must not prevent a
    // graphics/gameplay hardware test from running.
    webeast::wiiu::HornPlayer horn;
    initHorn(horn, sdRoot);

    GameWorldConfig config{};
    config.car.enabled = loadedMap == 2;
    config.props.enabled = loadedMap == 2;

    GameWorld world(0x57424541u);
    world.reset(1, config);

    bool inGame = false;
    bool optionsOpen = false;
    std::uint32_t selectedItem = 0; // 0 = PLAY, 1 = OPTIONS
    std::uint32_t lastCarWarningSerial = world.car().warningSerial;

    while (WHBProcIsRunning()) {
        PlayerInput input{};
        VPADStatus status{};
        VPADReadError error = VPAD_READ_SUCCESS;
        const bool hasInput =
            VPADRead(VPAD_CHAN_0, &status, 1, &error) > 0 &&
            error == VPAD_READ_SUCCESS;

        if (hasInput) {
            if (!inGame) {
                if (optionsOpen) {
                    if ((status.trigger & VPAD_BUTTON_B) != 0) {
                        optionsOpen = false;
                    }
                } else {
                    if ((status.trigger & (VPAD_BUTTON_UP | VPAD_BUTTON_DOWN)) != 0) {
                        selectedItem = selectedItem == 0 ? 1u : 0u;
                    }
                    if ((status.trigger & VPAD_BUTTON_A) != 0) {
                        if (selectedItem == 0) {
                            world.reset(1, config);
                            lastCarWarningSerial = world.car().warningSerial;
                            inGame = true;
                        } else {
                            optionsOpen = true;
                        }
                    }
                }
            } else {
                input.moveX = applyDeadzone(status.leftStick.x);
                input.moveZ = -applyDeadzone(status.leftStick.y);
                input.jumpPressed = (status.trigger & VPAD_BUTTON_A) != 0;

                if ((status.trigger & VPAD_BUTTON_MINUS) != 0) {
                    world.reset(1, config);
                    lastCarWarningSerial = world.car().warningSerial;
                }
                if ((status.trigger & VPAD_BUTTON_PLUS) != 0) {
                    inGame = false;
                    optionsOpen = false;
                }
            }
        }

        if (inGame) {
            world.update(1.0f / 60.0f, &input, 1);

            const std::uint32_t warningSerial = world.car().warningSerial;
            if (warningSerial != lastCarWarningSerial) {
                lastCarWarningSerial = warningSerial;
                if (warningSerial != 0) horn.play();
            }

            renderer.draw(world);
        } else {
            renderer.drawTitleScreen(selectedItem, optionsOpen);
        }

        OSSleepTicks(OSMillisecondsToTicks(1));
    }

    horn.shutdown();
    renderer.shutdown();
    WHBUnmountSdCard();
    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
