cmake_minimum_required(VERSION 3.16)
project(brick_breaker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/enet
)

file(GLOB SOURCES "src/*.cpp")

add_executable(game ${SOURCES})

target_link_libraries(game raylib enet m pthread dl)
