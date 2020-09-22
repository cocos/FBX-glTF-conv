
#include <bee/Convert/SceneConverter.h>
#include <fmt/format.h>

namespace bee {
template <typename T_> static T_ getMetalnessFromSpecular(const T_ *specular_) {
  return static_cast<T_>(specular_[0] > 0.5 ? 1 : 0);
}

std::optional<GLTFBuilder::XXIndex>
SceneConverter::_convertMaterial(fbxsdk::FbxSurfaceMaterial &fbx_material_) {
  if (fbx_material_.Is<fbxsdk::FbxSurfaceLambert>()) {
    return _convertLambertMaterial(
        static_cast<fbxsdk::FbxSurfaceLambert &>(fbx_material_));
  } else {
    return {};
  }
}

std::optional<GLTFBuilder::XXIndex> SceneConverter::_convertLambertMaterial(
    fbxsdk::FbxSurfaceLambert &fbx_material_) {
  const auto materialName = std::string{fbx_material_.GetName()};

  fx::gltf::Material glTFMaterial;
  glTFMaterial.name = materialName;
  auto &glTFPbrMetallicRoughness = glTFMaterial.pbrMetallicRoughness;

  // Transparency color
  std::array<float, 3> transparentColor = {1.0f, 1.0f, 1.0f};
  {
    const auto fbxTransparencyFactor = fbx_material_.TransparencyFactor.Get();
    if (const auto glTFOpacityTextureIndex =
            _convertTextureProperty(fbx_material_.TransparentColor)) {
      _warn(fmt::format(
          "Material \"{}\" use texture for property \"{}\", which is not supported.",
          materialName, "Transparent color"));
    } else {
      const auto fbxTransparentColor = fbx_material_.TransparentColor.Get();
      for (int i = 0; i < 3; ++i) {
        transparentColor[i] =
            static_cast<float>(fbxTransparentColor[i] * fbxTransparencyFactor);
      }
    }
    // FBX color is RGB, so we calculate the A channel as the average of the FBX
    // transparency color
    glTFPbrMetallicRoughness.baseColorFactor[3] =
        1.0f - std::accumulate(transparentColor.begin(), transparentColor.end(),
                               0.0f) /
                   3.0f;
  }

  // Base color
  {
    const auto diffuseFactor = fbx_material_.DiffuseFactor.Get();
    for (int i = 0; i < 3; ++i) {
      glTFPbrMetallicRoughness.baseColorFactor[i] =
          static_cast<float>(diffuseFactor);
    }
    if (const auto glTFDiffuseTextureIndex =
            _convertTextureProperty(fbx_material_.Diffuse)) {
      glTFPbrMetallicRoughness.baseColorTexture.index =
          *glTFDiffuseTextureIndex;
    } else {
      const auto fbxDiffuseColor = fbx_material_.Diffuse.Get();
      for (int i = 0; i < 3; ++i) {
        // TODO: should we multiply with transparent color?
        glTFPbrMetallicRoughness.baseColorFactor[i] =
            static_cast<float>(fbxDiffuseColor[i] * diffuseFactor);
      }
    }
  }

  // Normal map
  if (const auto glTFNormalMapIndex =
          _convertTextureProperty(fbx_material_.NormalMap)) {
    glTFMaterial.normalTexture.index = *glTFNormalMapIndex;
  }

  // Bump map
  if (const auto glTFBumpMapIndex =
          _convertTextureProperty(fbx_material_.Bump)) {
    glTFMaterial.normalTexture.index = *glTFBumpMapIndex;
  }

  // Emissive
  {
    const auto emissiveFactor = fbx_material_.EmissiveFactor.Get();
    if (const auto glTFEmissiveTextureIndex =
            _convertTextureProperty(fbx_material_.Emissive)) {
      glTFMaterial.emissiveTexture.index = *glTFEmissiveTextureIndex;
      for (int i = 0; i < 3; ++i) {
        glTFMaterial.emissiveFactor[i] = static_cast<float>(emissiveFactor);
      }
    } else {
      const auto emissive = fbx_material_.Emissive.Get();
      for (int i = 0; i < 3; ++i) {
        glTFMaterial.emissiveFactor[i] =
            static_cast<float>(emissive[i] * emissiveFactor);
      }
    }
  }

  if (fbx_material_.Is<fbxsdk::FbxSurfacePhong>()) {
    const auto &fbxPhong =
        static_cast<const fbxsdk::FbxSurfacePhong &>(fbx_material_);

    // Metallic factor
    auto fbxSpecular = fbxPhong.Specular.Get();
    std::array<fbxsdk::FbxDouble, 3> specular{fbxSpecular[0], fbxSpecular[1],
                                              fbxSpecular[2]};
    const auto fbxSpecularFactor = fbxPhong.SpecularFactor.Get();
    for (auto &c : specular) {
      c *= fbxSpecularFactor;
    }
    glTFPbrMetallicRoughness.metallicFactor =
        0.4f; // static_cast<float>(_getMetalnessFromSpecular(specular.data()));

    auto getRoughness = [&](float shininess_) {
      return std::sqrt(2.0f / (2.0f + shininess_));
    };

    // Roughness factor
    glTFPbrMetallicRoughness.roughnessFactor =
        getRoughness(static_cast<float>(fbxPhong.Shininess.Get()));
  }

  auto glTFMaterailIndex =
      _glTFBuilder.add(&fx::gltf::Document::materials, std::move(glTFMaterial));
  return glTFMaterailIndex;
}
} // namespace bee