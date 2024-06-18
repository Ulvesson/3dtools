cmake_minimum_required(VERSION 3.5)
project(3D-Tools
    VERSION 0.1.0
    LANGUAGES CXX)

add_subdirectory(extern)

add_executable(test
    src/test.cpp
)
