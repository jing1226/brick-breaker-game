cmake_minimum_required(VERSION 3.10)
project(breakout)

set(CMAKE_CXX_STANDARD 11)
add_executable(breakout
    main.cpp
    Ball.cpp
    Brick.cpp
    Paddle.cpp
    powerup.cpp
    tool.cpp
    enet/enet.c
)


include_directories(enet/include)

find_package(raylib REQUIRED)
target_link_libraries(breakout raylib)

if (WIN32)
    target_link_libraries(breakout ws2_32 winmm)
endif()