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

bool makeSdContentPath(char* out, std::size_t outSize,
                       const char* sdRoot, const char* fileName,
                       bool fallback) {
    if (!out || !sdRoot || !fileName) return false;
    const int written = fallback
        ? std::snprintf(out, outSize, "%s/wut/content/%s", sdRoot, fileName)
        : std::snprintf(out, outSize, "%s/wiiu/apps/webeast/content/%s", sdRoot, fileName);
    return written > 0 && static_cast<std::size_t>(written) < outSize;
}

char* readContentFile(const char* fileName,
                      const char* sdRoot,
                      std::uint32_t* outSize) {
    if (!fileName) return nullptr;

    // In an Aroma WUHB, RPXLoadingModule redirects the bundle's /content
    // directory to /vol/content. WHBReadWholeFile automatically prefixes
    // relative paths with /vol/content, so try the bundled asset first.
    if (char* bytes = WHBReadWholeFile(fileName, outSize)) {
        return bytes;
    }

    // Keep standalone RPX compatibility for older/manual SD packages.
    if (!sdRoot) return nullptr;
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeSdContentPath(path, sizeof(path), sdRoot, fileName,
                               fallback != 0)) {
            continue;
        }
        if (char* bytes = WHBReadWholeFile(path, outSize)) {
            return bytes;
        }
    }
    return nullptr;
}

bool loadWbmFile(WbmMesh& mesh, const char* sdRoot, const char* fileName) {
    std::uint32_t size = 0;
    char* bytes = readContentFile(fileName, sdRoot, &size);
    if (!bytes) return false;
    const bool ok = mesh.loadFromMemory(bytes, size);
    WHBFreeWholeFile(bytes);
    return ok;
}

int loadPreferredMap(WbmMesh& mesh, const char* sdRoot) {
    if (loadWbmFile(mesh, sdRoot, "map_02.wbm")) return 2;
    if (loadWbmFile(mesh, sdRoot, "map_01.wbm")) return 1;
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
    // Relative paths resolve to /vol/content, which is exactly where Aroma
    // exposes a WUHB's bundled content.
    if (renderer.init("pos_col_shader.gsh", mapMesh,
                      smallBoxMesh, bigBoxMesh, explosiveBarrelMesh)) {
        return true;
    }

    if (!sdRoot) return false;
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeSdContentPath(path, sizeof(path), sdRoot,
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
    // Audio is optional, but prefer the copy bundled in the WUHB.
    if (horn.init("car_honk.pcm", 16000)) return true;

    if (!sdRoot) return false;
    char path[512]{};
    for (int fallback = 0; fallback < 2; ++fallback) {
        if (!makeSdContentPath(path, sizeof(path), sdRoot,
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

    // A WUHB does not need a second SD mount to access its bundled content.
    // Mounting is only kept as an optional fallback for standalone RPX builds.
    const bool sdMounted = WHBMountSdCard();
    const char* sdRoot = sdMounted ? WHBGetSdCardMountPath() : nullptr;

    WbmMesh mapMesh;
    const int loadedMap = loadPreferredMap(mapMesh, sdRoot);
    if (loadedMap == 0) {
        if (sdMounted) WHBUnmountSdCard();
        WHBGfxShutdown();
        WHBProcShutdown();
        return -3;
    }

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
    config.car.enabled = loadedMap == 2;
    config.props.enabled = loadedMap == 2;

    GameWorld world(0x57424541u);
    world.reset(1, config);

    bool inGame = false;
    bool optionsOpen = false;
    std::uint32_t selectedItem = 0;
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
    if (sdMounted) WHBUnmountSdCard();
    WHBGfxShutdown();
    WHBProcShutdown();
    return 0;
}
