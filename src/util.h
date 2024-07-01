#ifndef UTIL_H
#define UTIL_H

#include <CesiumGltf/Model.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace util {

bool writeFile(const std::filesystem::path& filename, const std::vector<std::byte> buffer) {
    std::filesystem::create_directories(filename.parent_path());
    std::ofstream file(filename, std::ios::binary | std::ios::out);
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    file.close();
    return true;
}

std::vector<std::byte> readFile(const std::filesystem::path& fileName) {
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return buffer;
}

void saveImages(const CesiumGltf::Model& model) {
    static int imgNum = 1;

    for (const auto& image : model.images) {
        std::stringstream ss;
        ss << "/home/tomhe/work/images/img_" << imgNum++  << ".jpg";
        auto& bufferView = model.bufferViews.at(image.bufferView);
        auto& buffer = model.buffers.at(bufferView.buffer).cesium.data;

        //auto pos = buffer.begin() + bufferView.byteOffset;
        auto pos = (const char*)(buffer.data() + bufferView.byteOffset);

        std::ofstream of(/*std::filesystem::temp_directory_path() /*/ ss.str(), std::ios::out | std::ofstream::binary);
        //std::copy(pos, pos + bufferView.byteLength, std::ostreambuf_iterator<char>(of));
        of.write(pos, bufferView.byteLength);
        of.close();
    }
}

}

#endif // UTIL_H
