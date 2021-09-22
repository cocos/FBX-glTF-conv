
#pragma once

#include <bee/Convert/FbxMeshVertexLayout.h>
#include <bee/Convert/GLTFSamplerHash.h>
#include <bee/Convert/NeutralType.h>
#include <bee/Converter.h>
#include <bee/GLTFBuilder.h>
#include <bee/GLTFUtilities.h>
#include <bee/polyfills/filesystem.h>
#include <compare>
#include <fbxsdk.h>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace bee {
inline std::u8string_view forceTreatAsU8(std::string_view s_) {
  return {reinterpret_cast<const char8_t *>(s_.data()), s_.size()};
}

inline std::string_view forceTreatAsPlain(std::u8string_view s_) {
  return {reinterpret_cast<const char *>(s_.data()), s_.size()};
}

class SceneConverter {
public:
  SceneConverter(fbxsdk::FbxManager &fbx_manager_,
                 fbxsdk::FbxScene &fbx_scene_,
                 const ConvertOptions &options_,
                 std::u8string_view fbx_file_name_,
                 GLTFBuilder &glTF_builder_);

  void convert();

private:
  struct FbxBlendShapeData {
    struct Channel {
      /// <summary>
      /// Blend shape deformer index.
      /// </summary>
      int blendShapeIndex;

      /// <summary>
      /// Channel index.
      /// </summary>
      int blendShapeChannelIndex;
      std::string name;
      fbxsdk::FbxDouble deformPercent;
      std::vector<std::tuple<fbxsdk::FbxShape *, fbxsdk::FbxDouble>>
          targetShapes;
    };

    std::vector<Channel> channels;

    std::vector<fbxsdk::FbxShape *> getShapes() const {
      std::vector<fbxsdk::FbxShape *> shapes;
      for (const auto &channelData : channels) {
        for (auto &[fbxShape, weight] : channelData.targetShapes) {
          shapes.push_back(fbxShape);
        }
      }
      return shapes;
    }

    std::vector<std::string> getShapeNames() const {
      std::vector<std::string> shapeNames;
      for (const auto &channelData : channels) {
        for (auto &[fbxShape, weight] : channelData.targetShapes) {
          shapeNames.push_back(channelData.name);
        }
      }
      return shapeNames;
    }
  };

  struct MeshSkinData {
    struct Bone {
      std::uint32_t glTFNode;
      fbxsdk::FbxAMatrix inverseBindMatrix;

      struct IBMSpreader;
    };

    struct InfluenceChannel {
      std::vector<NeutralVertexJointComponent> joints;
      std::vector<NeutralVertexWeightComponent> weights;
    };

    std::string name;

    std::vector<Bone> bones;

    std::vector<InfluenceChannel> channels;
  };

  struct NodeMeshesSkinData {
    std::string name;

    std::vector<MeshSkinData::Bone> bones;

    /// <summary>
    /// Channels of each mesh.
    /// </summary>
    std::vector<std::vector<MeshSkinData::InfluenceChannel>> meshChannels;
  };

  struct FbxNodeMeshesBumpMeta {
    std::vector<fbxsdk::FbxMesh *> meshes;

    struct BlendShapeDumpMeta {
      std::vector<FbxBlendShapeData> blendShapeDatas;
    };

    std::optional<BlendShapeDumpMeta> blendShapeMeta;
  };

  struct FbxNodeDumpMeta {
    std::uint32_t glTFNodeIndex;
    std::optional<FbxNodeMeshesBumpMeta> meshes;
  };

  struct ConvertMeshResult {
    GLTFBuilder::XXIndex glTFMeshIndex;
    std::optional<GLTFBuilder::XXIndex> glTFSkinIndex;
  };

  struct VertexBulk {
    using ChannelWriter =
        std::function<void(std::byte *out_, const std::byte *in_)>;

    struct Channel {
      std::string name;
      fx::gltf::Accessor::Type type;
      fx::gltf::Accessor::ComponentType componentType;
      std::size_t inOffset;
      std::uint32_t outOffset;
      ChannelWriter writer;
      std::optional<std::uint32_t> target;
    };

    std::optional<std::uint32_t> morphTargetHint;
    std::uint32_t stride;
    std::list<Channel> channels;
    bool vertexBuffer = false;

    void addChannel(const std::string &name_,
                    fx::gltf::Accessor::Type type_,
                    fx::gltf::Accessor::ComponentType component_type_,
                    std::uint32_t in_offset_,
                    ChannelWriter writer_,
                    std::optional<std::uint32_t> target_ = {}) {
      channels.emplace_back(Channel{name_, type_, component_type_, in_offset_,
                                    stride, writer_, target_});
      stride += countBytes(type_, component_type_);
    }
  };

  struct AnimRange {
  private:
    fbxsdk::FbxTime::EMode timeMode;
    fbxsdk::FbxLongLong firstFrame;
    /// <summary>
    /// Last frame(inclusive).
    /// </summary>
    fbxsdk::FbxLongLong lastFrame;

  public:
    AnimRange(fbxsdk::FbxTime::EMode time_mode_,
              fbxsdk::FbxLongLong first_frame_,
              fbxsdk::FbxLongLong last_frame_)
        : timeMode(time_mode_), firstFrame(first_frame_),
          lastFrame(last_frame_) {
    }

    fbxsdk::FbxLongLong frames_count() const {
      return lastFrame - firstFrame + 1;
    }

    fbxsdk::FbxDouble first_frame_seconds() const {
      return at(0).GetSecondDouble();
    }

    fbxsdk::FbxTime at(fbxsdk::FbxLongLong frame_index_) const {
      const auto fbxFrame = firstFrame + frame_index_;
      fbxsdk::FbxTime fbxTime;
      fbxTime.SetFrame(fbxFrame, timeMode);
      return fbxTime;
    }
  };

  struct MorphAnimation {
    std::vector<double> times;
    std::vector<double> values;
  };

  struct TextureContext {
    std::unordered_map<std::string, std::uint32_t> channel_index_map;

    bool compatible_with(const TextureContext &that_) const {
      const auto &lhs = this->channel_index_map;
      const auto &rhs = that_.channel_index_map;
      for (const auto &[k, v] : lhs) {
        if (const auto r = rhs.find(k); r != rhs.end() && r->second != v) {
          return false;
        }
      }
      return true;
    }
  };

  struct MaterialUsage {
    bool hasTransparentVertex = false;

    TextureContext texture_context;

    bool operator==(const MaterialUsage &that_) const {
      return this->hasTransparentVertex == that_.hasTransparentVertex &&
             this->texture_context.compatible_with(that_.texture_context);
    }
  };

  struct MaterialConvertKey {
  public:
    MaterialConvertKey(const fbxsdk::FbxSurfaceMaterial &material_,
                       const MaterialUsage &usage_)
        : _material(material_.GetUniqueID()), _usage(usage_) {
    }

    bool operator==(const MaterialConvertKey &that_) const {
      return this->_material == that_._material && this->_usage == that_._usage;
    }

    struct Hash {
      std::size_t operator()(const MaterialConvertKey &key_) const noexcept {
        return std::hash<fbxsdk::FbxUInt64>{}(key_._material);
      }
    };

  private:
    fbxsdk::FbxUInt64 _material;
    MaterialUsage _usage;
  };

  GLTFBuilder &_glTFBuilder;
  fbxsdk::FbxManager &_fbxManager;
  fbxsdk::FbxGeometryConverter _fbxGeometryConverter;
  fbxsdk::FbxScene &_fbxScene;
  const ConvertOptions &_options;
  const std::u8string _fbxFileName;
  fbxsdk::FbxTime::EMode _animationTimeMode = fbxsdk::FbxTime::EMode::eFrames24;
  std::map<fbxsdk::FbxUInt64, GLTFBuilder::XXIndex> _fbxNodeMap;
  std::vector<fbxsdk::FbxNode *> _anncouncedfbxNodes;
  std::unordered_map<GLTFSamplerKeys, GLTFBuilder::XXIndex, GLTFSamplerHash>
      _uniqueSamplers;
  std::unordered_map<MaterialConvertKey,
                     std::optional<GLTFBuilder::XXIndex>,
                     MaterialConvertKey::Hash>
      _materialConvertCache;
  std::unordered_map<fbxsdk::FbxUInt64,
                     std::optional<fx::gltf::Material::Texture>>
      _textureMap;
  std::unordered_map<const fbxsdk::FbxNode *, FbxNodeDumpMeta> _nodeDumpMetaMap;
  std::optional<fbxsdk::FbxDouble> _unitScaleFactor = 1.0;

  inline fbxsdk::FbxVector4
  _applyUnitScaleFactorV3(const fbxsdk::FbxVector4 &v_) const {
    return _unitScaleFactor ? fbxsdk::FbxVector4{v_[0] * (*_unitScaleFactor),
                                                 v_[1] * (*_unitScaleFactor),
                                                 v_[2] * (*_unitScaleFactor)}
                            : v_;
  }

  /// <summary>
  /// Prefer std::u8string_view overloading.
  /// </summary>
  void _log(bee::Logger::Level level_, const char8_t *message_) {
    _log(level_, std::u8string_view{message_});
  }

  void _log(bee::Logger::Level level_, const std::u8string &message_) {
    _log(level_, std::u8string_view{message_});
  }

  void _log(bee::Logger::Level level_, std::u8string_view message_);

  void _log(bee::Logger::Level level_, Json &&message_);

  fbxsdk::FbxGeometryConverter &_getGeometryConverter();

  void _prepareScene();

  void _announceNodes(const fbxsdk::FbxScene &fbx_scene_);

  void _announceNode(fbxsdk::FbxNode &fbx_node_);

  void _setNodeMap(const fbxsdk::FbxNode &fbx_node_,
                   GLTFBuilder::XXIndex glTF_node_index_);

  std::optional<GLTFBuilder::XXIndex>
  _getNodeMap(const fbxsdk::FbxNode &fbx_node_);

  std::string _convertName(const char *fbx_name_);

  bee::filesystem::path _convertFileName(const char *fbx_file_name_);

  GLTFBuilder::XXIndex _convertScene(fbxsdk::FbxScene &fbx_scene_);

  void _convertNode(fbxsdk::FbxNode &fbx_node_);

  std::string _getName(fbxsdk::FbxNode &fbx_node_);

  std::optional<ConvertMeshResult>
  _convertNodeMeshes(FbxNodeDumpMeta &node_meta_,
                     const std::vector<fbxsdk::FbxMesh *> &fbx_meshes_,
                     fbxsdk::FbxNode &fbx_node_);

  std::string _getName(fbxsdk::FbxMesh &fbx_mesh_, fbxsdk::FbxNode &fbx_node_);

  std::tuple<fbxsdk::FbxMatrix, fbxsdk::FbxMatrix>
  _getGeometrixTransform(fbxsdk::FbxNode &fbx_node_);

  fx::gltf::Primitive _convertMeshAsPrimitive(
      fbxsdk::FbxMesh &fbx_mesh_,
      std::string_view mesh_name_,
      fbxsdk::FbxMatrix *vertex_transform_,
      fbxsdk::FbxMatrix *normal_transform_,
      std::span<fbxsdk::FbxShape *> fbx_shapes_,
      std::span<MeshSkinData::InfluenceChannel> skin_influence_channels_,
      MaterialUsage &material_usage_);

  FbxMeshVertexLayout _getFbxMeshVertexLayout(
      fbxsdk::FbxMesh &fbx_mesh_,
      std::span<fbxsdk::FbxShape *> fbx_shapes_,
      std::span<MeshSkinData::InfluenceChannel> skin_influence_channels_);

  fx::gltf::Primitive _createPrimitive(std::list<VertexBulk> &bulks_,
                                       std::uint32_t target_count_,
                                       std::uint32_t vertex_count_,
                                       std::byte *untyped_vertices_,
                                       std::uint32_t vertex_size_,
                                       std::span<std::uint32_t> indices_,
                                       std::string_view primitive_name_);

  std::list<VertexBulk>
  _typeVertices(const FbxMeshVertexLayout &vertex_layout_);

  fbxsdk::FbxSurfaceMaterial *_getTheUniqueMaterial(fbxsdk::FbxMesh &fbx_mesh_);

  /// <summary>
  /// Things get even more complicated if there are more than one mesh attached to a node.
  /// Usually this happened since the only mesh originally attached use multiple materials
  /// and we have to split it.
  ///
  /// Because limits of glTF. There can be only one skin bound to all primitives of a mesh.
  /// So we here try to merge all skin data of each mesh into one.
  /// The main task is to remap joints in each node mesh.
  /// As metioned above, if the multiple meshes were generated because of splitting,
  /// their corresponding joint should have equal inverse bind matrices.
  /// But if they are multiple in in nature, the inverse bind matrices may differ from each other.
  /// In such cases, we do warn.
  /// </summary>
  std::optional<NodeMeshesSkinData>
  _extractNodeMeshesSkinData(const std::vector<fbxsdk::FbxMesh *> &fbx_meshes_);

  std::optional<MeshSkinData>
  _extractSkinData(const fbxsdk::FbxMesh &fbx_mesh_);

  std::uint32_t _createGLTFSkin(const NodeMeshesSkinData &skin_data_);

  std::optional<FbxBlendShapeData>
  _extractdBlendShapeData(const fbxsdk::FbxMesh &fbx_mesh_);

  std::optional<FbxNodeMeshesBumpMeta::BlendShapeDumpMeta>
  _extractNodeMeshesBlendShape(
      const std::vector<fbxsdk::FbxMesh *> &fbx_meshes_);

  std::optional<GLTFBuilder::XXIndex>
  _convertMaterial(fbxsdk::FbxSurfaceMaterial &fbx_material_,
                   const MaterialUsage &material_usage_);

  std::optional<GLTFBuilder::XXIndex>
  _convertLambertMaterial(fbxsdk::FbxSurfaceLambert &fbx_material_,
                          const MaterialUsage &material_usage_);

  std::optional<GLTFBuilder::XXIndex>
  _convertUnknownMaterial(fbxsdk::FbxSurfaceMaterial &fbx_material_,
                          const MaterialUsage &material_usage_);

  std::optional<GLTFBuilder::XXIndex>
  _convertStanardMaterialProperties(fbxsdk::FbxSurfaceMaterial &fbx_material_,
                                    const MaterialUsage &material_usage_);

  std::optional<fx::gltf::Material::Texture>
  _convertTextureProperty(fbxsdk::FbxProperty &fbx_property_,
                          const TextureContext &texture_context_);

  std::optional<fx::gltf::Material::Texture>
  _convertFileTextureShared(fbxsdk::FbxFileTexture &fbx_file_texture_,
                            const TextureContext &texture_context_);

  std::optional<GLTFBuilder::XXIndex>
  _convertFileTexture(const fbxsdk::FbxFileTexture &fbx_texture_);

  std::optional<fx::gltf::Material::NormalTexture>
  _convertTexturePropertyAsNormalTexture(
      fbxsdk::FbxProperty &fbx_property_,
      const TextureContext &texture_context_);

  std::optional<fx::gltf::Material::NormalTexture>
  _convertFileTextureAsNormalTexture(fbxsdk::FbxFileTexture &fbx_file_texture_,
                                     const TextureContext &texture_context_);

  std::uint32_t _findUVIndex(const fbxsdk::FbxTexture &fbx_texture_,
                             const TextureContext &texture_context_);

  void
  _convertTextureUVTransform(const fbxsdk::FbxTexture &fbx_texture_,
                             fx::gltf::Material::Texture &glTF_texture_info_);

  static bool _hasValidImageExtension(const bee::filesystem::path &path_);

  std::optional<GLTFBuilder::XXIndex>
  _convertTextureSource(const fbxsdk::FbxFileTexture &fbx_texture_);

  std::optional<std::string> _searchImage(const std::string_view name_);

  std::optional<std::u8string> _processPath(const bee::filesystem::path &path_);

  static std::u8string _getMimeTypeFromExtension(std::u8string_view ext_name_);

  std::optional<GLTFBuilder::XXIndex>
  _convertTextureSampler(const fbxsdk::FbxFileTexture &fbx_texture_);

  fx::gltf::Sampler::WrappingMode
  _convertWrapMode(fbxsdk::FbxTexture::EWrapMode fbx_wrap_mode_);

  void _convertAnimation(fbxsdk::FbxScene &fbx_scene_);

  fbxsdk::FbxTimeSpan
  _getAnimStackTimeSpan(fbxsdk::FbxAnimStack &fbx_anim_stack_,
                        fbxsdk::FbxScene &fbx_scene_);

  void _convertAnimationLayer(fx::gltf::Animation &glTF_animation_,
                              fbxsdk::FbxAnimLayer &fbx_anim_layer_,
                              fbxsdk::FbxScene &fbx_scene_,
                              const AnimRange &anim_range_);

  void _convertAnimationLayer(fx::gltf::Animation &glTF_animation_,
                              fbxsdk::FbxAnimLayer &fbx_anim_layer_,
                              fbxsdk::FbxNode &fbx_node_,
                              const AnimRange &anim_range_);

  void _extractWeightsAnimation(fx::gltf::Animation &glTF_animation_,
                                fbxsdk::FbxAnimLayer &fbx_anim_layer_,
                                fbxsdk::FbxNode &fbx_node_,
                                const AnimRange &anim_range_);

  void _writeMorphAnimtion(fx::gltf::Animation &glTF_animation_,
                           const MorphAnimation &morph_animtion_,
                           std::uint32_t glTF_node_index_,
                           const fbxsdk::FbxNode &fbx_node_);

  MorphAnimation
  _extractWeightsAnimation(fbxsdk::FbxAnimLayer &fbx_anim_layer_,
                           const fbxsdk::FbxNode &fbx_node_,
                           fbxsdk::FbxMesh &fbx_mesh_,
                           const FbxBlendShapeData &blend_shape_data_,
                           const AnimRange &anim_range_);

  void _extractTrsAnimation(fx::gltf::Animation &glTF_animation_,
                            fbxsdk::FbxAnimLayer &fbx_anim_layer_,
                            fbxsdk::FbxNode &fbx_node_,
                            const AnimRange &anim_range_);
};
} // namespace bee
