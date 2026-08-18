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

bool loadWbmFile(WbmMesh& mesh, const char* sdRoot, const char* fileName) {
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
    // V0.1 solo combat intentionally boots Map 1 first. Map 2 is retained only
    // as a fallback so an older SD setup can still display a real map.
    if (loadWbmFile(mesh, sdRoot, "map_01.wbm")) return 1;
    if (loadWbmFile(mesh, sdRoot, "map_02.wbm")) return 2;
    return 0;
}

void loadMap2PropMeshes(WbmMesh& smallBox,
                        WbmMesh& bigBox,
                        WbmMesh& explosiveBarrel,
                        const char* sdRoot) {
    loadWbmFile(smallBox, sdRoot, "prop_small_box.wbm");
    loadWbmFile(bigBox, sdRoot, "prop_big_box.wbm");
    loadWbmFile(explosiveBarrel, sdRoot, "prop_explosive_barrel.wbm");
}

bool initRenderer(webeast::wiiu::DebugRenderer& renderer,
                  const WbmMesh* mapMesh,
                  const WbmMesh* smallBoxMesh,
                  const WbmMesh* bigBoxMesh,
                  const WbmMesh* explosiveBarrelMesh,
                  const char* sdRoot) {
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeContentPath(path, sizeof(path), sdRoot,
                             "pos_col_shader.gsh", fallback != 0)) {
            continue;
        }
        if (renderer.init(path, mapMesh,
                          smallBoxMesh, bigBoxMesh, explosiveBarrelMesh)) {
            return true;
        }
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
    // If no WBM map is present, DebugRenderer deliberately draws its built-in
    // baseplate fallback. This keeps the combat test bootable from a fresh CI
    // package; copying the existing map_01.wbm onto the SD restores real Map 1.

    WbmMesh smallBoxMesh;
    WbmMesh bigBoxMesh;
    WbmMesh explosiveBarrelMesh;
    if (loadedMap == 2) {
        loadMap2PropMeshes(smallBoxMesh, bigBoxMesh, explosiveBarrelMesh, sdRoot);
    }

    webeast::wiiu::DebugRenderer renderer;
    if (!initRenderer(renderer,
                      &mapMesh,
                      &smallBoxMesh,
                      &bigBoxMesh,
                      &explosiveBarrelMesh,
                      sdRoot)) {
        WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -4;
    }

    webeast::wiiu::HornPlayer horn;
    initHorn(horn, sdRoot);

    GameWorldConfig config{};

    // V0.1 scope: Map 1 + one controllable Dummy + one stationary/no-AI
    // training Dummy. Ball, car and Map 2 props are intentionally disabled so
    // the hardware test is only about punch/grab/throw/falling.
    config.ballEnabled = false;
    config.car.enabled = false;
    config.props.enabled = false;
    config.combat.enabled = true;
    config.trainingDummy.enabled = true;
    config.trainingDummy.index = 1;

    GameWorld world(0x57424541u);
    world.reset(2, config);

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
                            world.reset(2, config);
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

                // V0.1 combat controls:
                // Y  = punch
                // ZR = hold to grab; releasing ZR throws the grabbed Dummy.
                input.punchPressed = (status.trigger & VPAD_BUTTON_Y) != 0;
                input.grabHeld = ((status.hold | status.trigger) & VPAD_BUTTON_ZR) != 0;

                if ((status.trigger & VPAD_BUTTON_MINUS) != 0) {
                    world.reset(2, config);
                    lastCarWarningSerial = world.car().warningSerial;
                }
                if ((status.trigger & VPAD_BUTTON_PLUS) != 0) {
                    inGame = false;
                    optionsOpen = false;
                }
            }
        }

        if (inGame) {
            // Only player 0 receives input. Player 1 is the stationary training
            // Dummy and is still simulated physically when hit/thrown.
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
