cmake_minimum_required(VERSION 3.5)
project(3D-Tools
    VERSION 0.1.0
    LANGUAGES CXX)

add_subdirectory(extern)

add_executable(test
    src/test.cpp
)

target_link_libraries(test PRIVATE
    Cesium3DTilesReader
    CesiumGltfReader
    CesiumGltfWriter
)

add_executable(processtile src/proc.cpp)

target_link_libraries(processtile PRIVATE
    Cesium3DTilesReader
    CesiumGltfReader
    CesiumGltfWriter
)
