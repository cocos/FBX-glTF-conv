
#include <cassert>
#include <bee/GLTFBuilder.h>

namespace bee {
GLTFBuilder::GLTFBuilder() {
  _bufferKeeps.emplace_back();
}

GLTFBuilder::BuildResult GLTFBuilder::build(BuildOptions options) {
  BuildResult buildResult;

  if (options.copyright) {
    _glTFDocument.asset.copyright = *options.copyright;
  }
  if (options.generator) {
    _glTFDocument.asset.generator = *options.generator;
  }

  const auto nBuffers = static_cast<std::uint32_t>(_bufferKeeps.size());
  _glTFDocument.buffers.resize(nBuffers);
  buildResult.buffers.resize(nBuffers);
  for (std::remove_const_t<decltype(nBuffers)> iBuffer = 0; iBuffer < nBuffers;
       ++iBuffer) {
    const auto &bufferKeep = _bufferKeeps[iBuffer];
    std::uint32_t bufferByteLength = 0;
    for (const auto &bufferViewKeep : bufferKeep.bufferViews) {
      bufferByteLength +=
          static_cast<std::uint32_t>(bufferViewKeep.data.size());
    }

    std::vector<std::byte> bufferStorage(bufferByteLength);
    std::uint32_t bufferOffset = 0;
    for (const auto &bufferViewKeep : bufferKeep.bufferViews) {
      auto &bufferView = _glTFDocument.bufferViews[bufferViewKeep.index];
      auto bufferViewSize =
          static_cast<std::uint32_t>(bufferViewKeep.data.size());
      bufferView.byteOffset = bufferOffset;
      bufferView.buffer = iBuffer;
      std::memcpy(bufferStorage.data() + bufferOffset,
                  bufferViewKeep.data.data(), bufferViewSize);
      bufferOffset += bufferViewSize;
    }
    buildResult.buffers[iBuffer] = std::move(bufferStorage);

    fx::gltf::Buffer glTFBuffer;
    glTFBuffer.byteLength = bufferByteLength;
    _glTFDocument.buffers[iBuffer] = glTFBuffer;
  }

  return buildResult;
}

const GLTFBuilder::BufferViewInfo GLTFBuilder::createBufferView(
    std::uint32_t byte_length_, std::uint32_t align_, XXIndex buffer_) {
  assert(buffer_ < _bufferKeeps.size());
  auto &bufferKeep = _bufferKeeps[buffer_];
  auto index = static_cast<std::uint32_t>(_glTFDocument.bufferViews.size());
  fx::gltf::BufferView bufferView;
  bufferView.byteLength = byte_length_;
  _glTFDocument.bufferViews.push_back(std::move(bufferView));
  std::vector<std::byte> data(byte_length_);
  auto pData = data.data();
  BufferViewKeep bufferViewKeep;
  bufferViewKeep.index = index;
  bufferViewKeep.align = align_;
  bufferViewKeep.data = std::move(data);
  bufferKeep.bufferViews.push_back(std::move(bufferViewKeep));
  BufferViewInfo bufferViewInfo;
  bufferViewInfo.data = pData;
  bufferViewInfo.index = index;
  return bufferViewInfo;
}

void GLTFBuilder::useExtension(std::string_view extension_name_) {
  std::string extName{extension_name_};
  if (_glTFDocument.extensionsUsed.cend() ==
      std::find(_glTFDocument.extensionsUsed.cbegin(),
                _glTFDocument.extensionsUsed.cend(), extName)) {
    _glTFDocument.extensionsUsed.push_back(std::move(extName));
  }
}
} // namespace bee