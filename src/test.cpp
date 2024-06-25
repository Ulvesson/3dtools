#include <Cesium3DTiles/Tileset.h>
#include <Cesium3DTilesReader/TilesetReader.h>
#include <CesiumGltfReader/GltfReader.h>
#include <CesiumGltfWriter/GltfWriter.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>

namespace fs = std::filesystem;

namespace {

struct input_data {
    std::filesystem::path tileset_path;
    std::filesystem::path workdir;
    std::filesystem::path texture;
};

bool writeFile(const std::filesystem::path& filename, const std::vector<std::byte> buffer) {
    fs::create_directories(filename.parent_path());
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

bool readContent(const std::filesystem::path& filename) {
    if (!std::filesystem::exists(filename)) {
        std::cout << "not found: " << filename.c_str() << std::endl;
        return false;
    }

    CesiumGltfReader::GltfReader reader;
    auto result = reader.readGltf(readFile(filename));

    if (result.errors.size()) {
        std::cout << "Errors: " << std::endl;
        return false;
    }

    for (const auto& warn : result.warnings) {
        std::cout << warn << std::endl;
    }

    const auto& model =result.model;

    if (!model.has_value()) {
        std::cout << "No model" << std::endl;
        return false;
    }

    saveImages(model.value());

    return true;
}

CesiumJsonReader::ReadJsonResult<Cesium3DTiles::Tileset> readTileset(const std::filesystem::path& filename)
{
    Cesium3DTilesReader::TilesetReader reader;
    return reader.readFromJson(readFile(filename));
}

class TilesetProcessor {
public:
    explicit TilesetProcessor(const fs::path& tileset, const fs::path& workdir, const fs::path& texture)
        : _tileset(tileset)
        , _workdir(workdir)
        , _texture(texture) {}

    bool checkInput() const {
        if (!std::filesystem::exists(_tileset)) {
            std::cout << "File not found: " << _tileset.c_str() << std::endl;
            return false;
        }
        if (!std::filesystem::exists(_workdir)) {
            std::cout << "Invalid work dir: " << _workdir.c_str() << std::endl;
            return false;
        }
        if (!std::filesystem::exists(_texture) || _texture.extension() != ".jpg") {
            std::cout << "Invalid texture: " << _texture.c_str() << std::endl;
            return false;
        }

        return true;
    }

    void processContent(const fs::path& root, const fs::path& tile) {
        CesiumGltfReader::GltfReader reader;
        auto result = reader.readGltf(readFile(root/tile));

        if (result.errors.size() > 0) {
            std::cout << "Error reading tile: " << tile.c_str() << std::endl;
            return;
        }
        auto model = result.model.value();
        if (model.images.size() > 0) {
            CesiumGltf::Buffer buffer;
            buffer.cesium.data = _texturedata;
            int32_t bufferIdx = model.buffers.size() + 1;
            model.buffers.push_back(buffer);
            CesiumGltf::BufferView bufferView;
            bufferView.buffer = bufferIdx;
            bufferView.byteOffset = 0;
            bufferView.byteLength = _texturedata.size();
            model.bufferViews.push_back(bufferView);
            int32_t bufferViewIdx = model.bufferViews.size();

            for (auto& image : model.images) {
                image.bufferView = bufferViewIdx;
            }
        }

        std::vector<std::byte> bufferData;
        for (auto& buf : model.buffers) {
            bufferData.insert(std::end(bufferData), std::begin(buf.cesium.data), std::end(buf.cesium.data));
        }

        CesiumGltfWriter::GltfWriter writer;
        auto writeResult = writer.writeGlb(model, gsl::span(bufferData));

        if (writeResult.errors.size() > 0) {
            return;
        }

        auto relative = fs::path(root/tile).lexically_normal().lexically_relative(_tileset.parent_path());
        writeFile(_workdir/relative, writeResult.gltfBytes);
    }

    int traverse(const std::filesystem::path& root, const Cesium3DTiles::Tileset& tileset)
    {
        if (tileset.root.content.has_value()) {
            const std::filesystem::path content(tileset.root.content->uri);

            if (content.extension() == ".json") {
                auto result = readTileset(root/content);
                if (result.errors.size() > 0) {
                    std::cout << "Invalid input: " << content.c_str();
                    return EXIT_FAILURE;
                }
                if (traverse((root/content).parent_path(), result.value.value()) == EXIT_FAILURE) {
                    return EXIT_FAILURE;
                }
            }
            else if (content.extension() == ".glb" /*&& !readContent(root / content)*/) {
                processContent(root, content);
            }
        }

        for (const auto& child : tileset.root.children) {
            traverse(root, child);
        }

        return EXIT_SUCCESS;
    }

    void traverse(const std::filesystem::path& root, const Cesium3DTiles::Tile& tile) {
        if (tile.content.has_value()) {
            const std::filesystem::path content(tile.content->uri);
            if (content.extension() == ".glb") {
                processContent(root, content);
            }
        }
        for (const auto& child : tile.children) {
            traverse(root, child);
        }
    }

    int run(const Cesium3DTiles::Tileset& tileset) {
        _texturedata = readFile(_texture);
        return traverse(_tileset.parent_path(), tileset);
    }

    const fs::path& getTilesetPath() const { return _tileset; }

private:
    const fs::path _tileset;
    const fs::path _workdir;
    const fs::path _texture;
    std::vector<std::byte> _texturedata;
};

void usage()
{
    std::cout << "Usage: test [INPUT FILE] [WORK DIR] [TEXTURE JPG] \n";
    exit(EXIT_FAILURE);
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 4) {
        usage();
    }

    TilesetProcessor processor(argv[1], argv[2], argv[3]);

    if (!processor.checkInput()) {
        return EXIT_FAILURE;
    }

    auto result = readTileset(processor.getTilesetPath());

    if (result.errors.size() > 0) {
        std::cout << "Invalid input: " << processor.getTilesetPath().c_str();
        return EXIT_FAILURE;
    }

    return processor.run(result.value.value());
}
