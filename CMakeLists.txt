cmake_minimum_required(VERSION 3.16)
project(WeBeastCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(webeast_core
    src/game/RandomBall.cpp
)

target_include_directories(webeast_core PUBLIC src)

add_executable(random_ball_sim tests/random_ball_sim.cpp)
target_link_libraries(random_ball_sim PRIVATE webeast_core)
