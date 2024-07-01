#include "util.h"
#include <CesiumGltfReader/GltfReader.h>
#include <CesiumGltfWriter/GltfWriter.h>
#include <iostream>

namespace fs = std::filesystem;
namespace {
std::optional<CesiumGltf::Model> readModel(const fs::path& filename) {
    CesiumGltfReader::GltfReader reader;
    auto result = reader.readGltf(util::readFile(filename));

    if (result.errors.size() > 0) {
        std::cout << "Error reading tile: " << filename.c_str() << std::endl;
    }

    return result.model;
}
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cout << "USAGE: processtile [INFILE] [OUTFILE]\n";
        return EXIT_FAILURE;
    }
    fs::path input(argv[1]);
    fs::path output(argv[2]);
    if (!fs::exists(input)) {
        std::cout << "Missing input: " << input.c_str() << std::endl;
        return EXIT_FAILURE;
    }
    auto modelOpt = readModel(input);
    if (!modelOpt.has_value()) {
        return EXIT_FAILURE;
    }
    const auto& model = modelOpt.value();

    CesiumGltfWriter::GltfWriter writer;
    auto writeResult = writer.writeGlb(model, gsl::span(model.buffers[0].cesium.data));

    if (writeResult.errors.size() > 0) {
        std::cout << "Error writing glb" << std::endl;
        return EXIT_FAILURE;
    }

    //auto relative = fs::path(root/tile).lexically_normal().lexically_relative(_tileset.parent_path());
    util::writeFile(output, writeResult.gltfBytes);
    return EXIT_SUCCESS;
}
