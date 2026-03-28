cmake_minimum_required(VERSION 3.20)
project(Xments LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(demo demo/main.cpp)

target_include_directories(demo PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(demo PRIVATE ${SDL2_LIBRARIES})

# Optional: enable warnings
target_compile_options(demo PRIVATE -Wall -Wextra -Wpedantic)
