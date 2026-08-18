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
                     int source) {
    if (!out || !fileName) return false;

    int written = -1;
    switch (source) {
    case 0:
        // Aroma/WUHB content bundled with wuhbtool --content is exposed here.
        written = std::snprintf(out, outSize, "fs:/vol/content/%s", fileName);
        break;
    case 1:
        if (!sdRoot) return false;
        written = std::snprintf(out, outSize,
                                "%s/wiiu/apps/webeast/content/%s",
                                sdRoot, fileName);
        break;
    case 2:
        if (!sdRoot) return false;
        written = std::snprintf(out, outSize,
                                "%s/wut/content/%s",
                                sdRoot, fileName);
        break;
    default:
        return false;
    }

    return written > 0 && static_cast<std::size_t>(written) < outSize;
}

bool readContentFile(const char* sdRoot,
                     const char* fileName,
                     char*& bytes,
                     std::uint32_t& size) {
    bytes = nullptr;
    size = 0;

    char path[512]{};
    for (int source = 0; source < 3; ++source) {
        if (!makeContentPath(path, sizeof(path), sdRoot, fileName, source)) {
            continue;
        }
        bytes = WHBReadWholeFile(path, &size);
        if (bytes) return true;
    }
    return false;
}

bool loadWbmFile(WbmMesh& mesh, const char* sdRoot, const char* fileName) {
    char* bytes = nullptr;
    std::uint32_t size = 0;
    if (!readContentFile(sdRoot, fileName, bytes, size)) return false;

    const bool ok = mesh.loadFromMemory(bytes, size);
    WHBFreeWholeFile(bytes);
    return ok;
}

int loadPreferredMap(WbmMesh& mesh, const char* sdRoot) {
    // V0.1 solo combat intentionally boots Map 1 first. A user's existing
    // map_01.wbm on SD is still picked up after checking WUHB bundled content.
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
    for (int source = 0; source < 3; ++source) {
        if (!makeContentPath(path, sizeof(path), sdRoot,
                             "pos_col_shader.gsh", source)) {
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
    for (int source = 0; source < 3; ++source) {
        if (!makeContentPath(path, sizeof(path), sdRoot,
                             "car_honk.pcm", source)) {
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

    // WUHB content does not depend on mounting the raw SD card. Keep the mount
    // optional so Aroma can still boot the built-in combat/baseplate test even
    // if external SD access is unavailable for any reason.
    const bool sdMounted = WHBMountSdCard();
    const char* sdRoot = sdMounted ? WHBGetSdCardMountPath() : nullptr;

    WbmMesh mapMesh;
    const int loadedMap = loadPreferredMap(mapMesh, sdRoot);
    // If no WBM map is present, DebugRenderer deliberately draws its built-in
    // baseplate fallback. This keeps the V0.1 test self-contained.

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
        if (sdMounted) WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -4;
    }

    webeast::wiiu::HornPlayer horn;
    initHorn(horn, sdRoot);

    GameWorldConfig config{};

    // V0.1 scope: Map 1/baseplate + one controllable Dummy + one stationary
    // training Dummy. Ball, car and Map 2 props are disabled.
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
    if (sdMounted) WHBUnmountSdCard();
    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
