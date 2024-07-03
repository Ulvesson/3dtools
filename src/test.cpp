#include "util.h"

#include <Cesium3DTiles/Tileset.h>
#include <Cesium3DTilesReader/TilesetReader.h>
#include <CesiumGltfReader/GltfReader.h>
#include <CesiumGltfWriter/GltfWriter.h>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include <iostream>
#include <queue>

namespace fs = std::filesystem;

namespace {

struct input_data {
  std::filesystem::path tileset_path;
  std::filesystem::path workdir;
  std::filesystem::path texture;
};

CesiumJsonReader::ReadJsonResult<Cesium3DTiles::Tileset>
readTileset(const std::filesystem::path& filename) {
  Cesium3DTilesReader::TilesetReader reader;
  return reader.readFromJson(util::readFile(filename));
}

class TilesetProcessor {
public:
  explicit TilesetProcessor(
      const fs::path& tileset,
      const fs::path& workdir,
      const fs::path& texture)
      : _tileset(tileset), _workdir(workdir), _texture(texture) {}

  void clearWorkdir() const {
    for (const fs::path& item : fs::recursive_directory_iterator(_workdir)) {
      if (item.extension() == ".glb")
        fs::remove(item);
    }
  }

  bool checkInput() const {
    if (!std::filesystem::exists(_tileset)) {
      std::cout << "File not found: " << _tileset.c_str() << std::endl;
      return false;
    }
    if (!std::filesystem::exists(_workdir)) {
      std::cout << "Invalid work dir: " << _workdir.c_str() << std::endl;
      return false;
    } else {
      clearWorkdir();
    }
    if (!std::filesystem::exists(_texture) || _texture.extension() != ".jpg") {
      std::cout << "Invalid texture: " << _texture.c_str() << std::endl;
      return false;
    }

    return true;
  }

  void clearImageData(CesiumGltf::Model& model) {
    for (auto& image : model.images) {
      auto& bufferView = model.bufferViews[image.bufferView];
      auto& buffer = model.buffers.at(bufferView.buffer);
      auto data = gsl::span(buffer.cesium.data)
                      .subspan(bufferView.byteOffset, bufferView.byteLength);
      std::fill(data.begin(), data.end(), std::byte(0));
    }
  }

  void processMeshes(CesiumGltf::Model& model) {
    for (auto& mesh : model.meshes) {
      for (auto& primitive : mesh.primitives) {
        int idx = primitive.attributes.at("POSITION");
        auto& bufferView = model.bufferViews[idx];
        auto& buffer = model.buffers.at(bufferView.buffer);
        auto data = gsl::span(buffer.cesium.data)
                        .subspan(bufferView.byteOffset, bufferView.byteLength);
        const int64_t byteStride = bufferView.byteStride.value();
        auto pos = data.begin();
        float min = 100000;
        float max = -100000;
        while (pos != data.end()) {
          glm::vec3* p = reinterpret_cast<glm::vec3*>(&pos[0]);
          min = p->z < min ? p->z : min;
          max = p->z > max ? p->z : max;
          pos += byteStride;
        }
        float z = (max - min) / 2.0 + min;
        pos = data.begin();
        while (pos != data.end()) {
          glm::vec3* p = reinterpret_cast<glm::vec3*>(&pos[0]);
          p->z = z;
          pos += byteStride;
        }
      }
    }
  }

  void processContent(const fs::path& root, const fs::path& tile) {
    CesiumGltfReader::GltfReader reader;
    auto result = reader.readGltf(util::readFile(root / tile));

    if (result.errors.size() > 0) {
      std::cout << "Error reading tile: " << tile.c_str() << std::endl;
      return;
    }
    auto model = result.model.value();
    std::vector<std::byte> bufferData;
    if (model.buffers.size() == 0) {
      std::cout << "Unsupported buffer count\n";
      return;
    }
    clearImageData(model);
    processMeshes(model);
    if (model.images.size() > 0) {
      bufferData.assign(
          model.buffers.at(0).cesium.data.cbegin(),
          model.buffers.at(0).cesium.data.cend());
      uint64_t offset = bufferData.size();
      uint64_t size = _texturedata.size();
      std::for_each_n(
          _texturedata.begin(),
          _texturedata.size(),
          [&bufferData](const auto& n) { bufferData.push_back(n); });
      model.buffers.at(0).byteLength = model.buffers.at(0).cesium.data.size();
      int32_t bufferViewIdx = model.bufferViews.size();
      CesiumGltf::BufferView bufferView;
      bufferView.buffer = model.buffers.size() - 1;
      bufferView.byteLength = size;
      bufferView.byteOffset = offset;
      model.bufferViews.push_back(bufferView);
      for (auto& image : model.images) {
        image.bufferView = bufferViewIdx;
      }
    }

    // for (auto& buf : model.buffers) {
    //     bufferData.insert(std::end(bufferData), std::begin(buf.cesium.data),
    //     std::end(buf.cesium.data));
    // }

    // if (model.buffers.size() != 1) {
    //     std::cout << "Buffers: " << model.buffers.size() << std::endl;
    // }

    if (bufferData.size() == 0) {
      std::cout << "Error writing glb, empty buffer" << std::endl;
      return;
    }

    CesiumGltfWriter::GltfWriter writer;
    auto writeResult = writer.writeGlb(model, gsl::span(bufferData));

    if (writeResult.errors.size() > 0) {
      std::cout << "Error writing glb" << std::endl;
      return;
    }

    auto relative = fs::path(root / tile)
                        .lexically_normal()
                        .lexically_relative(_tileset.parent_path());
    util::writeFile(_workdir / relative, writeResult.gltfBytes);
  }

  int traverse(
      const std::filesystem::path& root,
      const Cesium3DTiles::Tileset& tileset) {
    const std::filesystem::path content(tileset.root.content->uri);
    if (content.extension() == ".json") {
      auto result = readTileset(root / content);
      if (result.errors.size() > 0) {
        std::cout << "Invalid input: " << content.c_str();
        return EXIT_FAILURE;
      }
      if (traverse((root / content).parent_path(), result.value.value()) ==
          EXIT_FAILURE) {
        return EXIT_FAILURE;
      }
    } else if (content.extension() == ".glb") {
      processContent(root, content);
      _processedCount++;
    }

    std::queue<Cesium3DTiles::Tile> queue;
    for (auto& tile : tileset.root.children) {
      queue.push(tile);
    }

    while (queue.size() > 0) {
      auto tile = queue.front();
      queue.pop();

      if (tile.content.has_value()) {
        const fs::path contentPath(tile.content->uri);
        if (contentPath.extension() == ".glb") {
          processContent(root, contentPath);
          _processedCount++;
          std::cout << _processedCount << "\n";
        }
        // Early exit for now.
        else if (contentPath.extension() == ".json") {
          auto result = readTileset(root / contentPath);
          if (result.errors.size() > 0) {
            std::cout << "Invalid input: " << content.c_str();
            return EXIT_FAILURE;
          }
          if (traverse((root / content).parent_path(), result.value.value()) ==
              EXIT_FAILURE) {
            return EXIT_FAILURE;
          }
        }
      }

      for (auto& child : tile.children) {
        queue.push(child);
      }
    }

    return EXIT_SUCCESS;
  }

  int run(const Cesium3DTiles::Tileset& tileset) {
    _texturedata = util::readFile(_texture);
    return traverse(_tileset.parent_path(), tileset);
  }

  const fs::path& getTilesetPath() const { return _tileset; }
  uint64_t getProcessedCount() const { return _processedCount; }

private:
  const fs::path _tileset;
  const fs::path _workdir;
  const fs::path _texture;
  std::vector<std::byte> _texturedata;
  uint64_t _processedCount = 0;
};

void usage() {
  std::cout << "Usage: test [INPUT FILE] [WORK DIR] [TEXTURE JPG] \n";
  exit(EXIT_FAILURE);
}

} // namespace

int main(int argc, char* argv[]) {
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

  int status = processor.run(result.value.value());
  std::cout << "Processed " << processor.getProcessedCount() << " tiles."
            << std::endl;
  return status;
}
