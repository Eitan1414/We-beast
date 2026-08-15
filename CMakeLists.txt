cmake_minimum_required(VERSION 3.16)
project(WeBeast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(webeast_core
    src/game/RandomBall.cpp
    src/game/PlayerController.cpp
    src/game/GameWorld.cpp
)

target_include_directories(webeast_core PUBLIC src)

# Host-side simulations/tests. These stay buildable on PC so gameplay and
# physics rules can be checked without deploying to a Wii U for every change.
if(NOT COMMAND wut_create_rpx)
    enable_testing()

    add_executable(random_ball_sim tests/random_ball_sim.cpp)
    target_link_libraries(random_ball_sim PRIVATE webeast_core)
    add_test(NAME random_ball_sim COMMAND random_ball_sim)

    add_executable(game_world_sim tests/game_world_sim.cpp)
    target_link_libraries(game_world_sim PRIVATE webeast_core)
    add_test(NAME game_world_sim COMMAND game_world_sim)
endif()

# When configured through devkitPro's powerpc-eabi-cmake, WUT provides this
# helper. The first Wii U target is intentionally a hardware gameplay harness:
# GamePad input + fixed-step world + Random Ball. GX2 scene rendering follows.
if(COMMAND wut_create_rpx)
    add_executable(webeast_wiiu
        src/platform/wiiu/main.cpp
    )
    target_link_libraries(webeast_wiiu PRIVATE webeast_core)
    wut_create_rpx(webeast_wiiu)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/webeast_wiiu.rpx"
            DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
