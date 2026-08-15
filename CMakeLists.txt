cmake_minimum_required(VERSION 3.16)
project(WeBeast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(webeast_core
    src/assets/WbmMesh.cpp
    src/game/RandomBall.cpp
    src/game/PlayerController.cpp
    src/game/GameWorld.cpp
)

target_include_directories(webeast_core PUBLIC src)

# Host-side simulations/tests. These stay buildable on PC so gameplay and
# runtime asset parsing can be checked without deploying to a Wii U each time.
if(NOT COMMAND wut_create_rpx)
    enable_testing()

    add_executable(random_ball_sim tests/random_ball_sim.cpp)
    target_link_libraries(random_ball_sim PRIVATE webeast_core)
    add_test(NAME random_ball_sim COMMAND random_ball_sim)

    add_executable(game_world_sim tests/game_world_sim.cpp)
    target_link_libraries(game_world_sim PRIVATE webeast_core)
    add_test(NAME game_world_sim COMMAND game_world_sim)

    add_executable(wbm_mesh_sim tests/wbm_mesh_sim.cpp)
    target_link_libraries(wbm_mesh_sim PRIVATE webeast_core)
    add_test(NAME wbm_mesh_sim COMMAND wbm_mesh_sim)
endif()

# Native Wii U/WUT target. It now includes the Map 2 hazard audio path in
# addition to GX2 rendering and VPAD input.
if(COMMAND wut_create_rpx)
    add_executable(webeast_wiiu
        src/platform/wiiu/main.cpp
        src/platform/wiiu/DebugRenderer.cpp
        src/platform/wiiu/HornPlayer.cpp
    )

    # Current devkitPro CMake does not automatically add WUT's import-library
    # directory for extra RPLs named explicitly here (such as sndcore2).
    target_link_directories(webeast_wiiu PRIVATE
        "$ENV{DEVKITPRO}/wut/lib"
        "$ENV{DEVKITPRO}/portlibs/wiiu/lib"
    )
    target_link_libraries(webeast_wiiu PRIVATE webeast_core sndcore2)
    wut_create_rpx(webeast_wiiu)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/webeast_wiiu.rpx"
            DESTINATION "${CMAKE_INSTALL_PREFIX}")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/runtime/shaders/pos_col_shader.gsh"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/content")
endif()
