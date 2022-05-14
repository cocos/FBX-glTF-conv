
#include "./fbxsdk/String.h"
#include <bee/Convert/SceneConverter.h>
#include <bee/polyfills/filesystem.h>
#include <cppcodec/base64_default_rfc4648.hpp>
#include <fmt/format.h>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/trigonometric.hpp>
#include <regex>

namespace {
using glm_fbx_vec3 = glm::tvec3<fbxsdk::FbxDouble>;

using glm_fbx_vec4 = glm::tvec4<fbxsdk::FbxDouble>;

using glm_fbx_quat = glm::tquat<fbxsdk::FbxDouble>;

using glm_fbx_mat4 = glm::tmat4x4<fbxsdk::FbxDouble>;

glm_fbx_vec3 fbx_to_glm(const fbxsdk::FbxDouble3 &v_) {
  return {v_[0], v_[1], v_[2]};
}

glm_fbx_mat4 compute_fbx_texture_transform(glm_fbx_vec3 pivot_center_,
                                           glm_fbx_vec3 offset_,
                                           glm_fbx_quat rotation_,
                                           glm_fbx_vec3 rotation_pivot_,
                                           glm_fbx_vec3 scale_,
                                           glm_fbx_vec3 scale_pivot_) {
  offset_ *= scale_;

  const auto pivotTransform =
      glm::translate(glm::identity<glm_fbx_mat4>(), pivot_center_);
  const auto invPivotTransform =
      glm::translate(glm::identity<glm_fbx_mat4>(), -pivot_center_);

  const auto translation =
      glm::translate(glm::identity<glm_fbx_mat4>(), offset_);

  const auto rotation = glm::mat4_cast(rotation_);
  const auto rotationPivotTransform =
      glm::translate(glm::identity<glm_fbx_mat4>(), rotation_pivot_);
  const auto invRotationPivotTransform =
      glm::translate(glm::identity<glm_fbx_mat4>(), -rotation_pivot_);

  const auto scale = glm::scale(glm::identity<glm_fbx_mat4>(), scale_);
  const auto scalePivotTransform =
      glm::translate(glm::identity<glm_fbx_mat4>(), scale_pivot_);
  const auto invScalePivotTransform =
      glm::translate(glm::identity<glm_fbx_mat4>(), -scale_pivot_);

  return pivotTransform * translation *
         (rotationPivotTransform * rotation * invRotationPivotTransform) *
         (scalePivotTransform * scale * invScalePivotTransform) *
         invPivotTransform;
};
} // namespace

namespace bee {
std::optional<fx::gltf::Material::Texture>
SceneConverter::_convertTextureProperty(
    const fbxsdk::FbxProperty &fbx_property_,
    const TextureContext &texture_context_) {
  const auto fbxFileTexture =
      fbx_property_.GetSrcObject<fbxsdk::FbxFileTexture>();
  if (fbxFileTexture) {
    return _convertFileTextureShared(*fbxFileTexture, texture_context_);
  } else {
    const auto fbxLayeredTexture =
        fbx_property_.GetSrcObject<fbxsdk::FbxLayeredTexture>();
    if (fbxLayeredTexture) {
      _log(Logger::Level::verbose,
           fmt::format("The property {} is connected with a layered texture, "
                       "which is not supported "
                       "currently. It will be ignored.",
                       fbx_string_to_utf8_checked(
                           fbx_property_.GetHierarchicalName())));
    } else {
      const auto fbxTexture = fbx_property_.GetSrcObject<fbxsdk::FbxTexture>();
      if (fbxTexture) {
        _log(Logger::Level::verbose,
             fmt::format("The property {} is connected with an unknown type "
                         "texture. It will be ignored.",
                         fbx_string_to_utf8_checked(
                             fbx_property_.GetHierarchicalName())));
      }
    }
    return {};
  }
}

std::optional<fx::gltf::Material::Texture>
SceneConverter::_convertFileTextureShared(
    fbxsdk::FbxFileTexture &fbx_file_texture_,
    const TextureContext &texture_context_) {
  auto materialTexture = [&]() -> std::optional<fx::gltf::Material::Texture> {
    auto fbxTextureId = fbx_file_texture_.GetUniqueID();
    if (auto r = _textureMap.find(fbxTextureId); r != _textureMap.end()) {
      return r->second;
    } else {
      auto glTFTextureIndex = _convertFileTexture(fbx_file_texture_);
      if (!glTFTextureIndex) {
        _textureMap.emplace(fbxTextureId, std::nullopt);
        return std::nullopt;
      } else {
        fx::gltf::Material::Texture materialTexture;
        materialTexture.index = *glTFTextureIndex;

        _convertTextureUVTransform(fbx_file_texture_, materialTexture);

        _textureMap.emplace(fbxTextureId, materialTexture);
        return materialTexture;
      }
    }
  }();

  if (materialTexture) {
    const auto uvIndex = _findUVIndex(fbx_file_texture_, texture_context_);
    materialTexture->texCoord = static_cast<std::int32_t>(uvIndex);
  }

  return materialTexture;
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

std::optional<fx::gltf::Material::NormalTexture>
SceneConverter::_convertTexturePropertyAsNormalTexture(
    const fbxsdk::FbxProperty &fbx_property_,
    const TextureContext &texture_context_) {
  const auto materialTexture =
      _convertTextureProperty(fbx_property_, texture_context_);
  if (!materialTexture) {
    return std::nullopt;
  }
  fx::gltf::Material::NormalTexture normalTexture;
  static_cast<fx::gltf::Material::Texture &>(normalTexture) = *materialTexture;
  return normalTexture;
}

std::optional<fx::gltf::Material::NormalTexture>
SceneConverter::_convertFileTextureAsNormalTexture(
    fbxsdk::FbxFileTexture &fbx_file_texture_,
    const TextureContext &texture_context_) {
  const auto materialTexture =
      _convertFileTextureShared(fbx_file_texture_, texture_context_);
  if (!materialTexture) {
    return std::nullopt;
  }
  fx::gltf::Material::NormalTexture normalTexture;
  static_cast<fx::gltf::Material::Texture &>(normalTexture) = *materialTexture;
  return normalTexture;
}

std::uint32_t
SceneConverter::_findUVIndex(const fbxsdk::FbxTexture &fbx_texture_,
                             const TextureContext &texture_context_) {
  const auto fbxUVSet = fbx_texture_.UVSet.Get();
  if (fbxUVSet.IsEmpty() || fbxUVSet == "default") {
    return 0;
  }

  if (const auto rIndex = texture_context_.channel_index_map.find(
          std::string{fbxUVSet.operator const char *()});
      rIndex != texture_context_.channel_index_map.end()) {
    return rIndex->second;
  }

  _log(Logger::Level::warning,
       fmt::format("Texture {} specified an unrecognized uv set {}",
                   fbx_texture_.GetName(),
                   static_cast<const char *>(fbxUVSet)));
  return 0;
}

void SceneConverter::_convertTextureUVTransform(
    const fbxsdk::FbxTexture &fbx_texture_,
    fx::gltf::Material::Texture &glTF_texture_info_) {
  // The relation between 3ds Max Offet, Tiling and FBX translation scale:
  // Translation = 0.5 - (Offset + 0.5) * Tiling
  //             = 0.5 + (-Offset + -0.5) * Tiling
  // Scale       = Tiling
  //
  // which means:
  // - Given UVmax
  // - Flip it
  // - Cancel the pivot center (0.5, 0.5)
  // - Scale by tiling
  // - Re-apply the pivot center
  //  A displacement of one unit is equal to the texture's width after
  //  applying U scaling.
  const auto fbxTranslationU = fbx_texture_.GetTranslationU();
  const auto fbxTranslationV = fbx_texture_.GetTranslationV();
  const auto fbxScallingU = fbx_texture_.GetScaleU();
  const auto fbxScallingV = fbx_texture_.GetScaleV();
  const auto fbxScallingPivot = fbx_to_glm(fbx_texture_.ScalingPivot.Get());
  const auto fbxRotationU = fbx_texture_.GetRotationU();
  const auto fbxRotationV = fbx_texture_.GetRotationV();
  const auto fbxRotationW = fbx_texture_.GetRotationW();
  const auto fbxRotationPivot = fbx_to_glm(fbx_texture_.RotationPivot.Get());
  const auto fbxTranslation = glm::tvec3<fbxsdk::FbxDouble>(
      fbxTranslationU, fbxTranslationV, static_cast<fbxsdk::FbxDouble>(0.0));
  const auto fbxRotation = glm::quat_cast(
      glm::eulerAngleXYZ(glm::radians(fbxRotationU), glm::radians(fbxRotationV),
                         glm::radians(fbxRotationW)));
  const auto fbxScale = glm::tvec3<fbxsdk::FbxDouble>(
      fbxScallingU, fbxScallingV, static_cast<fbxsdk::FbxDouble>(1.0));

  const auto noTransform = fbxTranslation == glm::zero<glm_fbx_vec3>() &&
                           fbxRotation == glm::identity<glm_fbx_quat>() &&
                           fbxScale == glm::one<glm_fbx_vec3>() &&
                           fbxRotationPivot == glm::zero<glm_fbx_vec3>() &&
                           fbxScallingPivot == glm::zero<glm_fbx_vec3>();

  if (!noTransform) {
    // https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_texture_transform

    const auto glTFOffset = fbxTranslation;
    const auto glTFScale = fbxScale;

    // TODO: Not invalidated
    // const auto textureTransformMat = compute_fbx_texture_transform(
    //    glm::tvec3<fbxsdk::FbxDouble>(-0.5, -0.5, 0.0), fbxTranslation,
    //    fbxRotation,
    //    glm_fbx_vec3{fbxRotationPivot[0], fbxRotationPivot[1],
    //                 fbxRotationPivot[2]},
    //    fbxScale,
    //    glm_fbx_vec3{fbxScallingPivot[0], fbxScallingPivot[1],
    //                 fbxScallingPivot[2]});
    // glm_fbx_vec3 glTFOffset;
    // glm_fbx_vec3 glTFScale;
    // glm_fbx_quat glTFRotation;
    // glm_fbx_vec3 _skew;
    // glm_fbx_vec4 _pers;
    // const auto decomposeSuccess =
    //    glm::decompose(textureTransformMat, glTFScale, glTFRotation,
    //                   glTFOffset, _skew, _pers);
    // assert(decomposeSuccess);

    auto &khrTextureTransformExtension =
        glTF_texture_info_
            .extensionsAndExtras["extensions"]["KHR_texture_transform"];
    khrTextureTransformExtension["offset"] = Json::array(
        {static_cast<float>(glTFOffset.x), static_cast<float>(glTFOffset.y)});
    // TODO
    // khrTextureTransformExtension["rotation"] =
    // glm::radians(fbxRotationW);
    khrTextureTransformExtension["scale"] = Json::array(
        {static_cast<float>(glTFScale.x), static_cast<float>(glTFScale.y)});
  }
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
    const auto mimeType =
        _getMimeTypeFromExtension(normalizedPath.extension().u8string());
    const auto chars = fmt::format("data:{};base64,{}",
                                   forceTreatAsPlain(mimeType), base64Data);
    return std::u8string(reinterpret_cast<const char8_t *>(chars.data()));
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