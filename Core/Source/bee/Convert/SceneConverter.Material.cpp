
#include <bee/Convert/ConvertError.h>
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/String.h>
#include <fmt/format.h>
#include <glm/gtx/compatibility.hpp>
#include <glm/vec3.hpp>

namespace bee {

namespace {
template <typename T>
using ValueAndTexture = std::tuple<T, fbxsdk::FbxFileTexture *>;

using FbxDoubleValueAndTexture = ValueAndTexture<fbxsdk::FbxDouble>;

using FbxDouble4ValueAndTexture = ValueAndTexture<fbxsdk::FbxDouble4>;

template <typename T>
void extract_property(const fbxsdk::FbxProperty parent_property_,
                      std::string_view property_name_,
                      T &value_) {
  const auto prop = parent_property_.FindHierarchical(property_name_.data());
  if (prop.IsValid()) {
    value_ = prop.Get<T>();
  }
}

void extract_texture(const fbxsdk::FbxProperty parent_property_,
                     std::string_view property_name_,
                     fbxsdk::FbxFileTexture *&value_) {
  const auto textureProperty =
      parent_property_.Find(property_name_.data(), false);
  if (!textureProperty.IsValid()) {
    return;
  }
  const auto isEnabled = parent_property_.Find(
      (std::string{property_name_} + "_on").c_str(), false);
  if (isEnabled.IsValid() && !isEnabled.Get<fbxsdk::FbxBool>()) {
    return;
  }
  const auto fbxTexture =
      textureProperty.GetSrcObject<fbxsdk::FbxFileTexture>();
  value_ = fbxTexture;
}

template <typename T>
void extract_property(const fbxsdk::FbxProperty parent_property_,
                      std::string_view property_name_,
                      std::string_view texture_property_name_,
                      ValueAndTexture<T> &value_tex_) {
  auto &[value, map] = value_tex_;
  extract_property(parent_property_, property_name_, value);
  extract_texture(parent_property_, texture_property_name_, map);
}

constexpr fbxsdk::FbxDouble fbx_double_0 = 0.0;
constexpr fbxsdk::FbxDouble fbx_double_1 = 1.0;

template <typename T> struct SpecularGlossiness {
  glm::tvec3<T> diffuse;
  T opacity;
  glm::tvec3<T> specular;
  T shininess_exponent;

  T specular_intensity() const {
    return specular.x * 0.2125 + specular.y * 0.7154 + specular.z * 0.0721;
  }

  T diffuse_brighness() const {
    return std::pow(diffuse.x, 2) * 0.299 + std::pow(diffuse.y, 2) * 0.587 +
           std::pow(diffuse.z, 2) * 0.114;
  }

  T specular_brighness() const {
    return std::pow(specular.x, 2) * 0.299 + std::pow(specular.y, 2) * 0.587 +
           std::pow(specular.z, 2) * 0.114;
  }

  T specular_strength() const {
    return std::max({specular.x, specular.y, specular.z});
  }
};

template <typename T> struct MetallicRoughness {
  glm::tvec3<T> baseColor;
  T opacity;
  T metallic;
  T roughness;
};

template <typename T> constexpr static T dielectric_specular = 0.04;

template <typename T> constexpr static T epsilon = 1e-4;

template <typename T>
T solve_metallic(T diffuse_, T specular_, T one_minus_specular_strength_) {
  if (specular_ < dielectric_specular<T>) {
    return 0.0;
  }

  const auto a = dielectric_specular<T>;
  const auto b =
      diffuse_ * one_minus_specular_strength_ / (1.0 - a) + specular_ - 2.0 * a;
  const auto c = a - specular_;
  const auto D = b * b - 4.0 * a * c;
  const auto squareRoot = std::sqrt(std::max(0.0, D));
  return std::clamp((-b + squareRoot) / (2.0 * a), 0.0, 1.0);
}

template <typename T>
MetallicRoughness<T>
sg_to_mr(const SpecularGlossiness<T> &specular_glossiniess_) {
  // https://docs.microsoft.com/en-us/azure/remote-rendering/reference/material-mapping

  const auto diffuse = specular_glossiniess_.diffuse;
  const auto opacity = specular_glossiniess_.opacity;
  const auto specular = specular_glossiniess_.specular;

  const auto oneMinusSpecularStrength =
      1.0 - specular_glossiniess_.specular_strength();

  const auto roughness =
      std::sqrt(2.0 / (specular_glossiniess_.shininess_exponent *
                           specular_glossiniess_.specular_intensity() +
                       2.0));

  const auto metallic = solve_metallic(
      specular_glossiniess_.diffuse_brighness(),
      specular_glossiniess_.specular_brighness(), oneMinusSpecularStrength);

  const auto baseColorFromDiffuse =
      diffuse * (oneMinusSpecularStrength / (1.0 - dielectric_specular<T>) /
                 std::max(1.0 - metallic, epsilon<T>));
  const auto baseColroFromSpecular =
      (specular - dielectric_specular<T> * (1.0 - metallic)) *
      (1.0 / std::max(metallic, epsilon<T>));
  const auto baseColor =
      glm::clamp(glm::lerp(baseColorFromDiffuse, baseColroFromSpecular,
                           metallic * metallic),
                 glm::zero<glm::tvec3<T>>(), glm::one<glm::tvec3<T>>());

  MetallicRoughness<T> metallicRoughness;
  metallicRoughness.baseColor = baseColor;
  metallicRoughness.opacity = opacity;
  metallicRoughness.metallic = metallic;
  metallicRoughness.roughness = roughness;
  return metallicRoughness;
}

struct FbxSurfaceMaterialStandardProperties {
public:
  fbxsdk::FbxString shading_mode = "unknown";
  fbxsdk::FbxDouble4 ambient_color = {
      static_cast<fbxsdk::FbxDouble>(0.4), static_cast<fbxsdk::FbxDouble>(0.4),
      static_cast<fbxsdk::FbxDouble>(0.4), fbx_double_1};
  fbxsdk::FbxDouble ambient_factor = 1.0;
  fbxsdk::FbxDouble4 diffuse_color = {
      static_cast<fbxsdk::FbxDouble>(0.4), static_cast<fbxsdk::FbxDouble>(0.4),
      static_cast<fbxsdk::FbxDouble>(0.4), fbx_double_1};
  fbxsdk::FbxDouble diffuse_factor = fbx_double_1;
  fbxsdk::FbxDouble3 specular_color = {static_cast<fbxsdk::FbxDouble>(0.5),
                                       static_cast<fbxsdk::FbxDouble>(0.5),
                                       static_cast<fbxsdk::FbxDouble>(0.5)};
  fbxsdk::FbxDouble specular_factor = fbx_double_1;
  fbxsdk::FbxDouble shininess_exponent =
      static_cast<fbxsdk::FbxDouble>(6.31179141998291);
  fbxsdk::FbxDouble4 transparency_color = {fbx_double_1, fbx_double_1,
                                           fbx_double_1, fbx_double_1};
  fbxsdk::FbxDouble transparency_factor = fbx_double_0;
  fbxsdk::FbxDouble4 emissive_color = {fbx_double_0, fbx_double_0, fbx_double_0,
                                       fbx_double_1};
  fbxsdk::FbxDouble emissive_factor = fbx_double_0;
  fbxsdk::FbxFileTexture *bump = nullptr;
  fbxsdk::FbxFileTexture *normal_map = nullptr;
  fbxsdk::FbxDouble bump_factor = fbx_double_1;

  static FbxSurfaceMaterialStandardProperties
  read_from(const fbxsdk::FbxSurfaceMaterial &fbx_material_) {
    FbxSurfaceMaterialStandardProperties properties;

    const auto &rootProperty = fbx_material_.RootProperty;
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sShadingModel,
                     properties.shading_mode);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sAmbient,
                     properties.ambient_color);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sAmbientFactor,
                     properties.ambient_factor);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sDiffuse,
                     properties.diffuse_color);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sDiffuseFactor,
                     properties.diffuse_factor);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sSpecular,
                     properties.specular_color);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sSpecularFactor,
                     properties.specular_factor);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sShininess,
                     properties.shininess_exponent);
    extract_property(rootProperty,
                     fbxsdk::FbxSurfaceMaterial::sTransparentColor,
                     properties.transparency_color);
    extract_property(rootProperty,
                     fbxsdk::FbxSurfaceMaterial::sTransparencyFactor,
                     properties.transparency_factor);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sEmissive,
                     properties.emissive_color);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sEmissiveFactor,
                     properties.emissive_factor);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sBump,
                     properties.bump);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sNormalMap,
                     properties.normal_map);
    extract_property(rootProperty, fbxsdk::FbxSurfaceMaterial::sBumpFactor,
                     properties.bump_factor);
    // TODO:
    // static const char* sReflection;
    // static const char* sReflectionFactor;
    // static const char* sDisplacementColor;
    // static const char* sDisplacementFactor;
    // static const char* sVectorDisplacementColor;
    // static const char* sVectorDisplacementFactor;

    return properties;
  }

  SpecularGlossiness<fbxsdk::FbxDouble> get_specular_glossiness() const {
    glm::tvec3<fbxsdk::FbxDouble> fbxTransparenyScaled = {
        transparency_color[0] * transparency_factor,
        transparency_color[1] * transparency_factor,
        transparency_color[2] * transparency_factor};
    glm::tvec3<fbxsdk::FbxDouble> diffuseColor;
    for (int i = 0; i < 3; ++i) {
      diffuseColor[i] =
          static_cast<float>(diffuse_color[i] * diffuse_factor *
                             (fbx_double_1 - fbxTransparenyScaled[i]));
    }
    // FBX color is RGB, so we calculate the A channel as the average of the FBX
    // transparency color
    const auto opacity =
        fbx_double_1 - (fbxTransparenyScaled.x + fbxTransparenyScaled.y +
                        fbxTransparenyScaled.z) /
                           static_cast<fbxsdk::FbxDouble>(3.0);

    SpecularGlossiness<fbxsdk::FbxDouble> specularGlossiness;
    specularGlossiness.diffuse = diffuseColor;
    specularGlossiness.opacity = opacity;
    specularGlossiness.shininess_exponent = shininess_exponent;
    specularGlossiness.specular = {
        specular_color[0] * specular_factor,
        specular_color[1] * specular_factor,
        specular_color[2] * specular_factor,
    };
    return specularGlossiness;
  }
};

} // namespace

template <typename FinalError_>
class MaterialError : public ErrorBase<FinalError_> {
public:
  MaterialError(std::u8string_view material_name_)
      : _materialName(material_name_) {
  }

  std::u8string_view material() const {
    return _materialName;
  }

private:
  std::u8string _materialName;
};

template <typename FinalError_>
void to_json(nlohmann::json &j_, const MaterialError<FinalError_> &error_) {
  to_json(j_, static_cast<const ErrorBase<FinalError_> &>(error_));
  j_["material"] = forceTreatAsPlain(error_.material());
}

/// <summary>
/// Material {} use texture for property \"{}\", which is not supported.
/// </summary>
class ShallNotBeTextureError : public MaterialError<ShallNotBeTextureError> {
public:
  constexpr static inline std::u8string_view code = u8"shall_not_be_texture";

  ShallNotBeTextureError(std::u8string_view material_name_,
                         std::u8string_view property_name_)
      : MaterialError(material_name_), _propertyName(property_name_) {
  }

  std::u8string_view property() const {
    return _propertyName;
  }

private:
  std::u8string _propertyName;
};

void to_json(nlohmann::json &j_, const ShallNotBeTextureError &error_) {
  to_json(j_,
          static_cast<const MaterialError<ShallNotBeTextureError> &>(error_));
  j_["property"] = forceTreatAsPlain(error_.property());
}

template <typename T_> static T_ getMetalnessFromSpecular(const T_ *specular_) {
  return static_cast<T_>(specular_[0] > 0.5 ? 1 : 0);
}

std::optional<GLTFBuilder::XXIndex>
SceneConverter::_convertMaterial(fbxsdk::FbxSurfaceMaterial &fbx_material_,
                                 const MaterialUsage &material_usage_) {
  MaterialConvertKey convertKey{fbx_material_, material_usage_};
  auto r = _materialConvertCache.find(convertKey);
  if (r == _materialConvertCache.end()) {
    const auto glTFMaterialIndex =
        [&]() -> std::optional<GLTFBuilder::XXIndex> {
      if (_options.export_raw_materials) {
        return _exportRawMaterial(fbx_material_, material_usage_);
      }
      if (fbx_material_.Is<fbxsdk::FbxSurfaceLambert>()) {
        return _convertLambertMaterial(
            static_cast<fbxsdk::FbxSurfaceLambert &>(fbx_material_),
            material_usage_);
      } else {
        return _convertUnknownMaterial(fbx_material_, material_usage_);
      }
    }();
    r = _materialConvertCache.emplace(convertKey, glTFMaterialIndex).first;
  }
  return r->second;
}

std::optional<GLTFBuilder::XXIndex>
SceneConverter::_exportRawMaterial(fbxsdk::FbxSurfaceMaterial &fbx_material_,
                                   const MaterialUsage &material_usage_) {
  const auto materialName = std::string{fbx_string_to_utf8_checked(fbx_material_.GetName())};

  fx::gltf::Material glTFMaterial;
  glTFMaterial.name = materialName;

  auto &rawExtra =
      glTFMaterial.extensionsAndExtras["extras"]["FBX-glTF-conv"]["raw"];

  auto &propertyRoot = rawExtra["properties"];

  const auto exportMaterialProperty = [&](const auto &fbx_property_,
                                          std::string_view as_) {
    using ValueType = typename std::decay_t<decltype(fbx_property_)>::ValueType;

    // const auto hasDefaultValue = !fbx_property_.Modified();
    ///*const auto hasDefaultValue = fbxsdk::FbxProperty::HasDefaultValue(
    //    const_cast<std::decay_t<decltype(fbx_property_)> &>(fbx_property_));*/
    // if (hasDefaultValue) {
    //  return;
    //}

    Json json;

    if constexpr (std::is_same_v<ValueType, fbxsdk::FbxDouble>) {
      json["value"] = fbx_property_.Get();
    } else if constexpr (std::is_same_v<ValueType, fbxsdk::FbxDouble3>) {
      const auto v = fbx_property_.Get();
      json["value"] = Json::array({v[0], v[1], v[2]});
    } else {
      // https://stackoverflow.com/questions/38304847/constexpr-if-and-static-assert
      static_assert(!std::is_same_v<ValueType, ValueType>,
                    "Unsupported material value type.");
    }

    const auto glTFTexture =
        _convertTextureProperty(fbx_property_, material_usage_.texture_context);
    if (glTFTexture) {
      json["texture"] = *glTFTexture;
    }

    propertyRoot[std::string{as_}] = json;
  };

  const auto exportRawSurface =
      [&](const fbxsdk::FbxSurfaceMaterial &fbx_material_) {
        propertyRoot["shadingModel"] =
            fbx_string_to_utf8_checked(fbx_material_.ShadingModel.Get());
      };

  const auto exportRawLambert =
      [&](const fbxsdk::FbxSurfaceLambert &fbx_material_) {
        exportRawSurface(fbx_material_);
        exportMaterialProperty(fbx_material_.Emissive, "emissive");
        exportMaterialProperty(fbx_material_.EmissiveFactor, "emissiveFactor");
        exportMaterialProperty(fbx_material_.Ambient, "ambient");
        exportMaterialProperty(fbx_material_.EmissiveFactor, "ambientFactor");
        exportMaterialProperty(fbx_material_.Diffuse, "diffuse");
        exportMaterialProperty(fbx_material_.DiffuseFactor, "diffuseFactor");
        exportMaterialProperty(fbx_material_.NormalMap, "normalMap");
        exportMaterialProperty(fbx_material_.Bump, "bump");
        exportMaterialProperty(fbx_material_.BumpFactor, "bumpFactor");
        exportMaterialProperty(fbx_material_.TransparentColor,
                               "transparentColor");
        exportMaterialProperty(fbx_material_.TransparencyFactor,
                               "transparencyFactor");
        exportMaterialProperty(fbx_material_.DisplacementColor,
                               "displacementColor");
        exportMaterialProperty(fbx_material_.DisplacementFactor,
                               "displacementFactor");
        exportMaterialProperty(fbx_material_.VectorDisplacementColor,
                               "vectorDisplacementColor");
        exportMaterialProperty(fbx_material_.VectorDisplacementFactor,
                               "vectorDisplacementFactor");
      };

  const auto exportRawPhong =
      [&](const fbxsdk::FbxSurfacePhong &fbx_material_) {
        exportRawLambert(fbx_material_);
        exportMaterialProperty(fbx_material_.Specular, "specular");
        exportMaterialProperty(fbx_material_.SpecularFactor, "specularFactor");
        exportMaterialProperty(fbx_material_.Shininess, "shininess");
        exportMaterialProperty(fbx_material_.Reflection, "reflection");
        exportMaterialProperty(fbx_material_.ReflectionFactor,
                               "reflectionFactor");
      };

  if (fbx_material_.Is<fbxsdk::FbxSurfacePhong>()) {
    rawExtra["type"] = "phong";
    exportRawPhong(static_cast<fbxsdk::FbxSurfacePhong &>(fbx_material_));
  } else if (fbx_material_.Is<fbxsdk::FbxSurfaceLambert>()) {
    rawExtra["type"] = "lambert";
    exportRawLambert(static_cast<fbxsdk::FbxSurfaceLambert &>(fbx_material_));
  } else {
    propertyRoot = _dumpMaterialProperties(fbx_material_, material_usage_);
  }

  const auto glTFMaterailIndex =
      _glTFBuilder.add(&fx::gltf::Document::materials, std::move(glTFMaterial));
  return glTFMaterailIndex;
}

std::optional<GLTFBuilder::XXIndex> SceneConverter::_convertLambertMaterial(
    fbxsdk::FbxSurfaceLambert &fbx_material_,
    const MaterialUsage &material_usage_) {
  const auto materialName = std::string{fbx_string_to_utf8_checked(fbx_material_.GetName())};

  auto forbidTextureProperty = [&](std::u8string_view property_name_) {
    _log(Logger::Level::warning,
         ShallNotBeTextureError{forceTreatAsU8(materialName), property_name_});
  };

  fx::gltf::Material glTFMaterial;
  glTFMaterial.name = materialName;
  auto &glTFPbrMetallicRoughness = glTFMaterial.pbrMetallicRoughness;

  // Transparency color
  std::array<float, 3> transparentColor = {1.0f, 1.0f, 1.0f};
  {
    const auto fbxTransparencyFactor = fbx_material_.TransparencyFactor.Get();
    if (const auto glTFOpacityTextureIndex = _convertTextureProperty(
            fbx_material_.TransparentColor, material_usage_.texture_context)) {
      forbidTextureProperty(u8"Transparent color");
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
    if (const auto glTFDiffuseTexture = _convertTextureProperty(
            fbx_material_.Diffuse, material_usage_.texture_context)) {
      glTFPbrMetallicRoughness.baseColorTexture = *glTFDiffuseTexture;
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
  if (const auto glTFNormalMap = _convertTexturePropertyAsNormalTexture(
          fbx_material_.NormalMap, material_usage_.texture_context)) {
    glTFMaterial.normalTexture = *glTFNormalMap;
  }

  // Bump map
  if (const auto glTFBumpMap = _convertTexturePropertyAsNormalTexture(
          fbx_material_.Bump, material_usage_.texture_context)) {
    glTFMaterial.normalTexture = *glTFBumpMap;
  }

  // Emissive
  {
    const auto emissiveFactor = fbx_material_.EmissiveFactor.Get();
    if (const auto glTFEmissiveTexture = _convertTextureProperty(
            fbx_material_.Emissive, material_usage_.texture_context)) {
      glTFMaterial.emissiveTexture = *glTFEmissiveTexture;
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

  // Alpha mode
  if (glTFPbrMetallicRoughness.baseColorFactor[3] < 1.0f ||
      material_usage_.hasTransparentVertex) {
    glTFMaterial.alphaMode = fx::gltf::Material::AlphaMode::Blend;
  }

  auto glTFMaterailIndex =
      _glTFBuilder.add(&fx::gltf::Document::materials, std::move(glTFMaterial));
  return glTFMaterailIndex;
}

std::optional<GLTFBuilder::XXIndex>
SceneConverter::_convertStanardMaterialProperties(
    fbxsdk::FbxSurfaceMaterial &fbx_material_,
    const MaterialUsage &material_usage_) {
  const auto standardProperties =
      FbxSurfaceMaterialStandardProperties::read_from(fbx_material_);

  const auto materialName = std::string{ fbx_string_to_utf8_checked(fbx_material_.GetName())};

  fx::gltf::Material glTFMaterial;
  glTFMaterial.name = materialName;
  auto &glTFPbrMetallicRoughness = glTFMaterial.pbrMetallicRoughness;

  glTFMaterial.emissiveFactor = {
      static_cast<float>(standardProperties.emissive_color[0] *
                         standardProperties.emissive_factor),
      static_cast<float>(standardProperties.emissive_color[1] *
                         standardProperties.emissive_factor),
      static_cast<float>(standardProperties.emissive_color[2] *
                         standardProperties.emissive_factor)};

  const auto specularGlossiness = standardProperties.get_specular_glossiness();

  const auto metallicRoughness = sg_to_mr(specularGlossiness);

  glTFPbrMetallicRoughness.baseColorFactor = {
      static_cast<float>(metallicRoughness.baseColor.x),
      static_cast<float>(metallicRoughness.baseColor.y),
      static_cast<float>(metallicRoughness.baseColor.z),
      static_cast<float>(metallicRoughness.opacity),
  };
  glTFPbrMetallicRoughness.metallicFactor =
      static_cast<float>(metallicRoughness.metallic);
  glTFPbrMetallicRoughness.roughnessFactor =
      static_cast<float>(metallicRoughness.roughness);

  // Normal map
  if (standardProperties.normal_map) {
    if (const auto glTFNormalMap = _convertFileTextureAsNormalTexture(
            *standardProperties.normal_map, material_usage_.texture_context)) {
      glTFMaterial.normalTexture = *glTFNormalMap;
    }
  }

  // Bump map
  if (standardProperties.bump) {
    if (const auto glTFBumpMap = _convertFileTextureAsNormalTexture(
            *standardProperties.bump, material_usage_.texture_context)) {
      glTFMaterial.normalTexture = *glTFBumpMap;
    }
  }

  auto glTFMaterailIndex =
      _glTFBuilder.add(&fx::gltf::Document::materials, std::move(glTFMaterial));
  return glTFMaterailIndex;
} // namespace bee

std::optional<GLTFBuilder::XXIndex> SceneConverter::_convertUnknownMaterial(
    fbxsdk::FbxSurfaceMaterial &fbx_material_,
    const MaterialUsage &material_usage_) {
  const auto standard =
      _convertStanardMaterialProperties(fbx_material_, material_usage_);
  if (!standard) {
    return {};
  }

  std::function<std::optional<Json>(const fbxsdk::FbxProperty &)> dumpProperty;

  const auto dumpCompoundProperty =
      [&](const fbxsdk::FbxProperty &fbx_property_) -> std::optional<Json> {
    Json json;
    auto child = fbx_property_.GetChild();
    for (; child.IsValid(); child = child.GetSibling()) {
      const auto childJson = dumpProperty(child);
      if (childJson) {
        json[static_cast<const char *>(child.GetName())] = *childJson;
      }
    }
    return json;
  };

  dumpProperty =
      [&](const fbxsdk::FbxProperty &fbx_property_) -> std::optional<Json> {
    const auto propertyDataType = fbx_property_.GetPropertyDataType();
    if (propertyDataType == fbxsdk::FbxDoubleDT) {
      return fbx_property_.Get<fbxsdk::FbxDouble>();
    } else if (propertyDataType == fbxsdk::FbxDouble2DT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxDouble2>();
      return Json::array({value[0], value[1]});
    } else if (propertyDataType == fbxsdk::FbxDouble3DT ||
               propertyDataType == fbxsdk::FbxColor3DT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxDouble3>();
      return Json::array({value[0], value[1], value[2]});
    } else if (propertyDataType == fbxsdk::FbxDouble4DT ||
               propertyDataType == fbxsdk::FbxColor4DT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxDouble4>();
      return Json::array({value[0], value[1], value[2], value[3]});
    } else if (propertyDataType == fbxsdk::FbxFloatDT) {
      return fbx_property_.Get<fbxsdk::FbxFloat>();
    } else if (propertyDataType == fbxsdk::FbxBoolDT) {
      return fbx_property_.Get<fbxsdk::FbxBool>();
    } else if (propertyDataType == fbxsdk::FbxIntDT) {
      return fbx_property_.Get<fbxsdk::FbxInt>();
    } else if (propertyDataType == fbxsdk::FbxUIntDT) {
      return fbx_property_.Get<fbxsdk::FbxUInt>();
    } else if (propertyDataType == fbxsdk::FbxBoolDT) {
      return fbx_property_.Get<fbxsdk::FbxBool>();
    } else if (propertyDataType == fbxsdk::FbxStringDT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxString>();
      // TODO: process NON-UTF8 strings
      return static_cast<const char *>(value);
    } else if (propertyDataType == fbxsdk::FbxReferenceDT) {
      const auto srcObjectCount = fbx_property_.GetSrcObjectCount();
      if (srcObjectCount == 0) {
        return {}; // nullptr?
      } else if (srcObjectCount == 1) {
        const auto fbxFileTexture =
            fbx_property_.GetSrcObject<fbxsdk::FbxFileTexture>();
        if (fbxFileTexture) {
          const auto glTFTexture = _convertFileTextureShared(
              *fbxFileTexture, material_usage_.texture_context);
          if (glTFTexture) {
            return *glTFTexture;
          } else {
            return {}; // Bad texture.
          }
        }
      }
      _log(Logger::Level::verbose,
           fmt::format(
               "Material property {} is invalid: can only reference to texture",
               fbx_property_.GetHierarchicalName().Buffer()));
      return {};
    } else if (propertyDataType == fbxsdk::FbxCompoundDT) {
      return dumpCompoundProperty(fbx_property_);
    } else {
      const auto propertyDataTypeName =
          fbxsdk::FbxGetDataTypeNameForIO(propertyDataType);
      _log(Logger::Level::verbose,
           fmt::format("Unknown property data type: {}", propertyDataTypeName));
      return {};
    }
  };

  auto &glTFMaterial =
      _glTFBuilder.get(&fx::gltf::Document::materials)[*standard];
  auto &originalMaterial =
      glTFMaterial
          .extensionsAndExtras["extras"]["FBX-glTF-conv"]["originalMaterial"];

  const auto properties = dumpCompoundProperty(fbx_material_.RootProperty);
  if (properties) {
    originalMaterial["properties"] = *properties;
  }

  return standard;
}

Json SceneConverter::_dumpMaterialProperties(
    const fbxsdk::FbxSurfaceMaterial &fbx_material_,
    const MaterialUsage &material_usage_) {
  std::function<std::optional<Json>(const fbxsdk::FbxProperty &, bool &)>
      dumpProperty;

  const auto tryGetConnectedTextures =
      [&](const fbxsdk::FbxProperty &fbx_property_) -> std::optional<Json> {
    const auto srcObjectCount =
        fbx_property_.GetSrcObjectCount<fbxsdk::FbxFileTexture>();

    if (srcObjectCount == 0) {
      return {};
    }

    if (srcObjectCount != 1) {
      _log(Logger::Level::verbose,
           fmt::format("Material property {} connected with multiple textures, "
                       "which we can not processed now.",
                       fbx_property_.GetHierarchicalName().Buffer()));
      return {};
    }

    const auto fbxFileTexture =
        fbx_property_.GetSrcObject<fbxsdk::FbxFileTexture>(0);
    assert(fbxFileTexture);
    const auto glTFTexture = _convertFileTextureShared(
        *fbxFileTexture, material_usage_.texture_context);
    if (glTFTexture) {
      return *glTFTexture;
    } else {
      return {}; // Bad texture.
    }
  };

  const auto dumpCompoundProperty =
      [&](const fbxsdk::FbxProperty &fbx_property_) -> std::optional<Json> {
    Json json;
    auto child = fbx_property_.GetChild();
    for (; child.IsValid(); child = child.GetSibling()) {
      const auto propertyName =
          std::string{static_cast<const char *>(child.GetName())};
      bool mayBeConnectedWithTexture = true;
      const auto childJson = dumpProperty(child, mayBeConnectedWithTexture);
      if (childJson) {
        Json p;
        p["value"] = *childJson;
        if (mayBeConnectedWithTexture) {
          const auto childJsonTexture = tryGetConnectedTextures(child);
          if (childJsonTexture) {
            p["texture"] = *childJsonTexture;
          }
        }
        json[propertyName] = p;
      }
    }
    return json;
  };

  dumpProperty =
      [&](const fbxsdk::FbxProperty &fbx_property_,
          bool &may_be_connected_with_texture_) -> std::optional<Json> {
    may_be_connected_with_texture_ = true;
    const auto propertyDataType = fbx_property_.GetPropertyDataType();
    if (propertyDataType == fbxsdk::FbxDoubleDT) {
      return fbx_property_.Get<fbxsdk::FbxDouble>();
    } else if (propertyDataType == fbxsdk::FbxDouble2DT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxDouble2>();
      return Json::array({value[0], value[1]});
    } else if (propertyDataType == fbxsdk::FbxDouble3DT ||
               propertyDataType == fbxsdk::FbxColor3DT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxDouble3>();
      return Json::array({value[0], value[1], value[2]});
    } else if (propertyDataType == fbxsdk::FbxDouble4DT ||
               propertyDataType == fbxsdk::FbxColor4DT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxDouble4>();
      return Json::array({value[0], value[1], value[2], value[3]});
    } else if (propertyDataType == fbxsdk::FbxFloatDT) {
      return fbx_property_.Get<fbxsdk::FbxFloat>();
    } else if (propertyDataType == fbxsdk::FbxBoolDT) {
      return fbx_property_.Get<fbxsdk::FbxBool>();
    } else if (propertyDataType == fbxsdk::FbxIntDT) {
      return fbx_property_.Get<fbxsdk::FbxInt>();
    } else if (propertyDataType == fbxsdk::FbxUIntDT) {
      return fbx_property_.Get<fbxsdk::FbxUInt>();
    } else if (propertyDataType == fbxsdk::FbxBoolDT) {
      return fbx_property_.Get<fbxsdk::FbxBool>();
    } else if (propertyDataType == fbxsdk::FbxStringDT) {
      const auto value = fbx_property_.Get<fbxsdk::FbxString>();
      return fbx_string_to_utf8_checked(value);
    } else if (propertyDataType == fbxsdk::FbxReferenceDT) {
      may_be_connected_with_texture_ = false;
      return tryGetConnectedTextures(fbx_property_);
    } else if (propertyDataType == fbxsdk::FbxCompoundDT) {
      return dumpCompoundProperty(fbx_property_);
    } else {
      const auto propertyDataTypeName =
          fbxsdk::FbxGetDataTypeNameForIO(propertyDataType);
      _log(Logger::Level::verbose,
           fmt::format("Unknown property data type: {}", propertyDataTypeName));
      return {};
    }
  };

  return dumpCompoundProperty(fbx_material_.RootProperty).value_or(Json{});
}
} // namespace bee