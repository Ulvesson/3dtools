#include <Cesium3DTiles/Tileset.h>
#include <Cesium3DTilesReader/TilesetReader.h>
#include <iostream>
#include <filesystem>
#include <fstream>

namespace {
const std::filesystem::path input = "/home/tomhe/data/helsingfors/vricon_3d_surface_model_3dtiles/data/tileset.json";

std::vector<std::byte> readFile(const std::filesystem::path& fileName) {
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return buffer;
}

CesiumJsonReader::ReadJsonResult<Cesium3DTiles::Tileset> read(const std::filesystem::path& filename)
{
    Cesium3DTilesReader::TilesetReader reader;
    return reader.readFromJson(readFile(filename));
}

void traverse(const Cesium3DTiles::Tile& tile) {
    if (tile.content.has_value()) {
        const std::filesystem::path content(tile.content->uri);
        if (content.extension() == ".glb") {
            std::cout << "Child content: " << content.c_str() << std::endl;
        }
    }
    for (const auto& child : tile.children) {
        traverse(child);
    }
}

void traverse(const std::filesystem::path& root, const std::filesystem::path& filename)
{
    if (filename.extension() == ".json") {
        std::cout << filename.filename().c_str() << std::endl;
    }

    auto result = read(root / filename);

    if (!result.errors.empty()) {
        std::cout << "Errors found\n";
        return;
    }

    const Cesium3DTiles::Tileset& tileset = result.value.value();
    if (tileset.root.content.has_value()) {
        const std::filesystem::path content(tileset.root.content->uri);

        if (content.extension() == ".glb") {
            std::cout << "Content: " << content.c_str() << std::endl;
        }
        else {
            traverse(root, content);
        }
    }

    for (const auto& child : tileset.root.children) {
        traverse(child);
    }
}

} // namespace

int main(int argc, char* argv[])
{
    traverse(input.parent_path(), input.filename());
    // auto result = read(filename);

    // if (!result.errors.empty()) {
    //     std::cout << "Errors found\n";
    //     return EXIT_FAILURE;
    // }

    // const Cesium3DTiles::Tileset& tileset = result.value.value();
    // std::cout << "Version: " << tileset.asset.version << std::endl;
    // std::cout << "Children: " << tileset.root.children.size() << std::endl;



    // std::filesystem::path contentPath(tileset.root.content->uri);
    // if (contentPath.extension() == ".json") {

    // }
    return EXIT_SUCCESS;
}
