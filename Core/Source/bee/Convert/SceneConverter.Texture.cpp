
#include <bee/Convert/SceneConverter.h>
#include <bee/polyfills/filesystem.h>
#include <cppcodec/base64_default_rfc4648.hpp>
#include <fmt/format.h>

namespace bee {
std::optional<GLTFBuilder::XXIndex>
SceneConverter::_convertTextureProperty(fbxsdk::FbxProperty &fbx_property_) {
  const auto fbxFileTexture =
      fbx_property_.GetSrcObject<fbxsdk::FbxFileTexture>();
  if (!fbxFileTexture) {
    const auto fbxTexture = fbx_property_.GetSrcObject<fbxsdk::FbxTexture>();
    if (fbxTexture) {
      _log(Logger::Level::verbose,
           u8"The property is texture but is not file texture. It's ignored.");
    }
    return {};
  } else {
    auto fbxTextureId = fbxFileTexture->GetUniqueID();
    if (auto r = _textureMap.find(fbxTextureId); r != _textureMap.end()) {
      return r->second;
    } else {
      auto glTFTextureIndex = _convertFileTexture(*fbxFileTexture);
      _textureMap.emplace(fbxTextureId, glTFTextureIndex);
      return glTFTextureIndex;
    }
  }
}

std::optional<GLTFBuilder::XXIndex> SceneConverter::_convertFileTexture(
    const fbxsdk::FbxFileTexture &fbx_texture_) {
  const auto textureName = fbx_texture_.GetName();

  fx::gltf::Texture glTFTexture;
  glTFTexture.name = textureName;

  if (auto glTFSamplerIndex = _convertTextureSampler(fbx_texture_)) {
    glTFTexture.sampler = *glTFSamplerIndex;
  }

  if (const auto glTFImageIndex = _convertTextureSource(fbx_texture_)) {
    glTFTexture.source = *glTFImageIndex;
  }

  auto glTFTextureIndex =
      _glTFBuilder.add(&fx::gltf::Document::textures, std::move(glTFTexture));
  return glTFTextureIndex;
}

bool SceneConverter::_hasValidImageExtension(
    const bee::filesystem::path &path_) {
  const auto extName = path_.extension().string();
  const std::array<std::string, 3> validExtensions{".jpg", ".jpeg", ".png"};
  return std::any_of(validExtensions.begin(), validExtensions.end(),
                     [&extName](const std::string &valid_extension_) {
                       return std::equal(
                           valid_extension_.begin(), valid_extension_.end(),
                           extName.begin(), extName.end(), [](char a, char b) {
                             return tolower(a) == tolower(b);
                           });
                     });
}

std::optional<GLTFBuilder::XXIndex> SceneConverter::_convertTextureSource(
    const fbxsdk::FbxFileTexture &fbx_texture_) {
  namespace fs = bee::filesystem;

  const auto imageName = fbx_texture_.GetName();
  const auto imageFileName = _convertFileName(fbx_texture_.GetFileName());
  const auto imageFileNameRelative =
      _convertFileName(fbx_texture_.GetRelativeFileName());

  std::optional<fs::path> imageFilePath;
  if (!imageFileNameRelative.empty()) {
    imageFilePath =
        fs::path(_fbxFileName).parent_path() / imageFileNameRelative;
  } else if (!imageFileName.empty()) {
    imageFilePath = imageFileName;
  }

  if (imageFilePath && !_options.textureResolution.disabled) {
    const auto exists = [&imageFilePath] {
      std::error_code err;
      const auto status = fs::status(*imageFilePath, err);
      return !err && status.type() == fs::file_type::regular;
    };
    if (!_hasValidImageExtension(*imageFilePath) || !exists()) {
      auto image = _searchImage(imageFilePath->stem().string());
      if (image) {
        imageFilePath = image;
      }
    }
  }

  if (imageFilePath && !_hasValidImageExtension(*imageFilePath)) {
    imageFilePath.reset();
  }

  fx::gltf::Image glTFImage;
  glTFImage.name = imageName;
  if (imageFilePath) {
    auto reference = _processPath(*imageFilePath);
    if (reference) {
      glTFImage.uri.assign(reference->begin(), reference->end());
    }
  }

  if (glTFImage.uri.empty()) {
    // Or we got `bufferView: 0`.
    // glTFImage.bufferView = -1;
    glTFImage.uri = "data:image/"
                    "png;base64,"
                    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42m"
                    "P8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg==";
  }

  glTFImage.extensionsAndExtras["extras"]["FBX-glTF-conv"]["fileName"] =
      forceTreatAsPlain(imageFileName.u8string());
  glTFImage.extensionsAndExtras["extras"]["FBX-glTF-conv"]["relativeFileName"] =
      forceTreatAsPlain(imageFileNameRelative.u8string());

  auto glTFImageIndex =
      _glTFBuilder.add(&fx::gltf::Document::images, std::move(glTFImage));

  return glTFImageIndex;
}

std::optional<std::string>
SceneConverter::_searchImage(const std::string_view name_) {
  namespace fs = bee::filesystem;
  for (const auto &location : _options.textureResolution.locations) {
    std::error_code err;
    fs::directory_iterator dirIter{fs::path{location}, err};
    if (err) {
      continue;
    }
    for (const auto &dirEntry : dirIter) {
      if (dirEntry.is_regular_file()) {
        if (auto path = dirEntry.path(); path.stem() == std::string{name_} &&
                                         _hasValidImageExtension(path)) {
          return path.string();
        }
      }
    }
  }
  return {};
}

std::optional<std::u8string>
SceneConverter::_processPath(const bee::filesystem::path &path_) {
  namespace fs = bee::filesystem;

  const auto getOutDirNormalized = [this]() {
    return fs::path(_options.out).parent_path().lexically_normal();
  };

  const auto normalizedPath = path_.lexically_normal();

  const auto toRelative = [getOutDirNormalized](const fs::path &to_) {
    const auto outDir = getOutDirNormalized();
    return to_.lexically_relative(outDir).generic_u8string();
  };

  switch (_options.pathMode) {
  case ConvertOptions::PathMode::absolute: {
    return normalizedPath.u8string();
  }
  case ConvertOptions::PathMode::relative: {
    return toRelative(normalizedPath);
  }
  case ConvertOptions::PathMode::strip: {
    const auto fileName = normalizedPath.filename();
    return fileName.u8string();
  }
  case ConvertOptions::PathMode::copy: {
    const auto outDir = getOutDirNormalized();
    const auto target = outDir / normalizedPath.filename();
    std::error_code err;
    fs::create_directories(target.parent_path(), err);
    if (err) {
      return {};
    }
    fs::copy_file(normalizedPath, target, fs::copy_options::overwrite_existing,
                  err);
    if (err) {
      return {};
    }
    return toRelative(target);
  }
  case ConvertOptions::PathMode::embedded: {
    std::ifstream ifstream(normalizedPath);
    if (!ifstream.good()) {
      return {};
    }
    std::vector<char> fileContent;
    ifstream.seekg(0, ifstream.end);
    auto fileSize =
        static_cast<decltype(fileContent)::size_type>(ifstream.tellg());
    ifstream.seekg(0, ifstream.beg);
    fileContent.resize(fileSize);
    ifstream.read(fileContent.data(), fileContent.size());
    const auto base64Data = cppcodec::base64_rfc4648::encode(
        reinterpret_cast<const char *>(fileContent.data()), fileContent.size());
    const auto base64DataU8 =
        std::u8string_view{reinterpret_cast<const char8_t *>(base64Data.data()),
                           base64Data.size()};
    const auto mimeType =
        _getMimeTypeFromExtension(normalizedPath.extension().u8string());
    return fmt::format(u8"data:{};base64,{}", mimeType, base64DataU8);
  }
  case ConvertOptions::PathMode::prefer_relative: {
    const auto relativePath =
        normalizedPath.lexically_relative(getOutDirNormalized());
    auto relativePathStr = relativePath.u8string();
    const auto dotdot = std::string_view{".."};
    const auto startsWithDotDot = [](const fs::path &path_) {
      for (const auto part : path_) {
        if (path_ == "..") {
          return true;
        }
        break;
      }
      return false;
    };
    if (relativePath.is_relative() && !startsWithDotDot(relativePath)) {
      return relativePathStr;
    } else {
      return normalizedPath.u8string();
    }
    break;
  }
  default: {
    assert(false);
    break;
  }
  }
  return {};
}

std::u8string
SceneConverter::_getMimeTypeFromExtension(std::u8string_view ext_name_) {
  auto lower = std::u8string{ext_name_};
  std::transform(lower.begin(), lower.end(), lower.begin(), [](auto c_) {
    return static_cast<decltype(c_)>(std::tolower(c_));
  });
  if (lower == u8".jpg" || lower == u8".jpeg") {
    return u8"image/jpeg";
  } else if (lower == u8".png") {
    return u8"image/png";
  } else {
    return u8"application/octet-stream";
  }
}

std::optional<GLTFBuilder::XXIndex> SceneConverter::_convertTextureSampler(
    const fbxsdk::FbxFileTexture &fbx_texture_) {
  GLTFSamplerKeys samplerKeys;
  samplerKeys.wrapS = _convertWrapMode(fbx_texture_.GetWrapModeU());
  samplerKeys.wrapT = _convertWrapMode(fbx_texture_.GetWrapModeV());

  auto r = _uniqueSamplers.find(samplerKeys);
  if (r == _uniqueSamplers.end()) {
    fx::gltf::Sampler glTFSampler;
    samplerKeys.set(glTFSampler);
    auto glTFSamplerIndex =
        _glTFBuilder.add(&fx::gltf::Document::samplers, std::move(glTFSampler));
    r = _uniqueSamplers.emplace(samplerKeys, glTFSamplerIndex).first;
  }

  return r->second;
}

fx::gltf::Sampler::WrappingMode
SceneConverter::_convertWrapMode(fbxsdk::FbxTexture::EWrapMode fbx_wrap_mode_) {
  switch (fbx_wrap_mode_) {
  case fbxsdk::FbxTexture::EWrapMode::eRepeat:
    return fx::gltf::Sampler::WrappingMode::Repeat;
  default:
    assert(fbx_wrap_mode_ == fbxsdk::FbxTexture::EWrapMode::eClamp);
    return fx::gltf::Sampler::WrappingMode::ClampToEdge;
  }
}
} // namespace bee