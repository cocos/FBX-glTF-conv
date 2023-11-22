
#include "./fbxsdk/SplitMeshByMaterial.h"
#include "./fbxsdk/String.h"
#include <bee/Convert/ConvertError.h>
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/Spreader.h>
#include <fmt/format.h>
#include <range/v3/all.hpp>

namespace bee {
/// <summary>
/// Node {} uses unsupported transform inheritance type '{}'.
/// </summary>
class UnsupportedInheritTypeError
    : public NodeError<UnsupportedInheritTypeError> {
public:
  constexpr static inline std::u8string_view code =
      u8"unsupported_inherit_type";

  enum class Type {
    RrSs,
    Rrs,
  };

  UnsupportedInheritTypeError(Type type_, std::string_view node_)
      : _type(type_), NodeError(node_) {
  }

  Type type() const {
    return _type;
  }

private:
  Type _type;
};

void to_json(nlohmann::json &j_,
             UnsupportedInheritTypeError::Type inherit_type_) {
  j_ = inherit_type_ == UnsupportedInheritTypeError::Type::Rrs ? "Rrs" : "RrSs";
}

void to_json(nlohmann::json &j_, const UnsupportedInheritTypeError &error_) {
  to_json(j_,
          static_cast<const NodeError<UnsupportedInheritTypeError> &>(error_));
  j_["type"] = error_.type();
}

SceneConverter::SceneConverter(fbxsdk::FbxManager &fbx_manager_,
                               fbxsdk::FbxScene &fbx_scene_,
                               const ConvertOptions &options_,
                               std::u8string_view fbx_file_name_,
                               GLTFBuilder &glTF_builder_)
    : _glTFBuilder(glTF_builder_), _fbxManager(fbx_manager_),
      _fbxScene(fbx_scene_), _options(options_), _fbxFileName(fbx_file_name_),
      _fbxGeometryConverter(&fbx_manager_) {
  const auto &globalSettings = fbx_scene_.GetGlobalSettings();
  if (!options_.animationBakeRate) {
    _animationTimeMode = globalSettings.GetTimeMode();
  } else {
    _animationTimeMode = fbxsdk::FbxTime::ConvertFrameRateToTimeMode(
        static_cast<double>(options_.animationBakeRate));
  }

  const auto frameRate = fbxsdk::FbxTime::GetFrameRate(_animationTimeMode);
  _log(Logger::Level::verbose, fmt::format("Frame rate: {}", frameRate));

  auto &documentExtras = glTF_builder_.document().extensionsAndExtras;
  documentExtras["extras"]["FBX-glTF-conv"]["animationFrameRate"] = frameRate;
}

void SceneConverter::convert() {
  _prepareScene();
  _announceNodes(_fbxScene);
  for (auto fbxNode : _anncouncedfbxNodes) {
    _convertNode(*fbxNode);
  }
  _convertScene(_fbxScene);
  _convertAnimation(_fbxScene);
}

void to_json(Json &j_, bee::Logger::Level level_) {
  j_ = static_cast<std::underlying_type_t<decltype(level_)>>(level_);
}

void SceneConverter::_log(bee::Logger::Level level_,
                          std::u8string_view message_) {
  if (_options.logger) {
    (*_options.logger)(level_, message_);
  }
}

void SceneConverter::_log(bee::Logger::Level level_, Json &&message_) {
  if (_options.logger) {
    (*_options.logger)(level_, std::move(message_));
  }
}

fbxsdk::FbxGeometryConverter &SceneConverter::_getGeometryConverter() {
  return _fbxGeometryConverter;
}

void SceneConverter::_prepareScene() {
  // Convert axis system
  fbxsdk::FbxAxisSystem::OpenGL.ConvertScene(&_fbxScene);

  // Convert system unit
  if (const auto fbxFileSystemUnit =
          _fbxScene.GetGlobalSettings().GetSystemUnit();
      fbxFileSystemUnit != fbxsdk::FbxSystemUnit::m) {
    // FBX SDK's `convertScene` is not perfectly.
    // The problem occurs when bind a FBX model under bones in another FBX
    // model. See: https://github.com/facebookincubator/FBX2glTF/issues/29
    // https://github.com/facebookincubator/FBX2glTF/pull/63
    if (_options.unitConversion ==
        ConvertOptions::UnitConversion::geometryLevel) {
      _unitScaleFactor.emplace(
          fbxsdk::FbxSystemUnit::m.GetConversionFactorFrom(fbxFileSystemUnit));
    } else if (_options.unitConversion ==
               ConvertOptions::UnitConversion::hierarchyLevel) {
      fbxsdk::FbxSystemUnit::ConversionOptions conversionOptions;
      conversionOptions.mConvertRrsNodes = true;
      conversionOptions.mConvertLimits = true;
      conversionOptions.mConvertClusters = true;
      conversionOptions.mConvertLightIntensity = true;
      conversionOptions.mConvertPhotometricLProperties = true;
      conversionOptions.mConvertCameraClipPlanes = true;
      fbxsdk::FbxSystemUnit::m.ConvertScene(&_fbxScene, conversionOptions);
    }
  }

  // Save Original mesh name
  _traverseNodes(_fbxScene.GetRootNode());

  // Trianglute the whole scene
  _fbxGeometryConverter.Triangulate(&_fbxScene, true);

  // Split meshes per material
  _splitMeshesResult = split_meshes_per_material(_fbxScene, _fbxGeometryConverter);
  if (_options.verbose) {
    for (const auto &splitItem : _splitMeshesResult) {
      _log(Logger::Level::verbose, fmt::format("Splitted {} into {}", splitItem.first->GetNameWithNameSpacePrefix().Buffer(), splitItem.second->GetNameWithNameSpacePrefix().Buffer()));
    }
  }
  if (const auto shouldFixSplittedMeshesName = _options.preserve_mesh_instances || _options.match_mesh_names) {
    for (const auto &splitItem : _splitMeshesResult) {
      splitItem.second->SetName(splitItem.first->GetName());
    }
  }
}

void SceneConverter::_traverseNodes(FbxNode *node) {
  auto mesh = node->GetMesh();
  if (mesh) {
    if (std::strlen(mesh->GetName()) > 0) {
      nodeMeshMap[node] = mesh->GetName();
    } else {
      nodeMeshMap[node] = node->GetName();
    }
  }
  for (int i = 0; i < node->GetChildCount(); i++) {
    _traverseNodes(node->GetChild(i));
  }
}

void SceneConverter::_announceNodes(const fbxsdk::FbxScene &fbx_scene_) {
  auto rootNode = fbx_scene_.GetRootNode();
  auto nChildren = rootNode->GetChildCount();
  for (auto iChild = 0; iChild < nChildren; ++iChild) {
    _announceNode(*rootNode->GetChild(iChild));
  }
}

void SceneConverter::_announceNode(fbxsdk::FbxNode &fbx_node_) {
  _anncouncedfbxNodes.push_back(&fbx_node_);
  fx::gltf::Node glTFNode;
  auto glTFNodeIndex =
      _glTFBuilder.add(&fx::gltf::Document::nodes, std::move(glTFNode));
  _setNodeMap(fbx_node_, glTFNodeIndex);

  auto nChildren = fbx_node_.GetChildCount();
  for (auto iChild = 0; iChild < nChildren; ++iChild) {
    _announceNode(*fbx_node_.GetChild(iChild));
  }
}

void SceneConverter::_setNodeMap(const fbxsdk::FbxNode &fbx_node_,
                                 GLTFBuilder::XXIndex glTF_node_index_) {
  _fbxNodeMap.emplace(fbx_node_.GetUniqueID(), glTF_node_index_);
}

std::optional<GLTFBuilder::XXIndex>
SceneConverter::_getNodeMap(const fbxsdk::FbxNode &fbx_node_) {
  auto r = _fbxNodeMap.find(fbx_node_.GetUniqueID());
  if (r == _fbxNodeMap.end()) {
    return {};
  } else {
    return r->second;
  }
}

std::string SceneConverter::_convertName(const char *fbx_name_) {
  return fbx_name_;
}

bee::filesystem::path
SceneConverter::_convertFileName(const char *fbx_file_name_) {
  std::u8string u8name{reinterpret_cast<const char8_t *>(fbx_file_name_)};
  // Some FBX files contain non-UTF8 encoded file names and will cause
  // `std::filesystem::path` to crash.
  // https://forums.autodesk.com/t5/fbx-forum/fbxfiletexture-getxfilename-may-return-incorrect-utf8-encoded/td-p/9957667
  try {
    return u8name;
  } catch (const std::exception &) {
    _log(Logger::Level::verbose,
         u8"The fbx file name is not correctly UTF8 encoded.");
    return {};
  }
}

GLTFBuilder::XXIndex
SceneConverter::_convertScene(fbxsdk::FbxScene &fbx_scene_) {
  if (_options.export_fbx_file_header_info) {
    const auto &fbxSceneInfo = *fbx_scene_.GetSceneInfo();

    auto &extensionsAndExtras =
        _glTFBuilder.get(&fx::gltf::Document::extensionsAndExtras);
    auto &sceneInfo = extensionsAndExtras["extras"]["FBX-glTF-conv"]
                                         ["fbxFileHeaderInfo"]["sceneInfo"];

    sceneInfo["url"] = fbx_string_to_utf8_checked(fbxSceneInfo.Url.Get());

    auto &original = sceneInfo["original"];
    original["applicationVendor"] = fbx_string_to_utf8_checked(
        fbxSceneInfo.Original_ApplicationVendor.Get());
    original["applicationName"] =
        fbx_string_to_utf8_checked(fbxSceneInfo.Original_ApplicationName.Get());
    original["applicationVersion"] = fbx_string_to_utf8_checked(
        fbxSceneInfo.Original_ApplicationVersion.Get());
    original["fileName"] =
        fbx_string_to_utf8_checked(fbxSceneInfo.Original_FileName.Get());

    sceneInfo["title"] = fbx_string_to_utf8_checked(fbxSceneInfo.mTitle);
    sceneInfo["subject"] = fbx_string_to_utf8_checked(fbxSceneInfo.mSubject);
    sceneInfo["author"] = fbx_string_to_utf8_checked(fbxSceneInfo.mAuthor);
    sceneInfo["keywords"] = fbx_string_to_utf8_checked(fbxSceneInfo.mKeywords);
    sceneInfo["revision"] = fbx_string_to_utf8_checked(fbxSceneInfo.mRevision);
    sceneInfo["comment"] = fbx_string_to_utf8_checked(fbxSceneInfo.mComment);
  }

  auto sceneName = _convertName(fbx_scene_.GetName());

  fx::gltf::Scene glTFScene;
  glTFScene.name = sceneName;

  auto rootNode = fbx_scene_.GetRootNode();
  auto nChildren = rootNode->GetChildCount();
  for (auto iChild = 0; iChild < nChildren; ++iChild) {
    auto glTFNodeIndex = _getNodeMap(*rootNode->GetChild(iChild));
    assert(glTFNodeIndex);
    glTFScene.nodes.push_back(*glTFNodeIndex);
  }

  auto glTFSceneIndex =
      _glTFBuilder.add(&fx::gltf::Document::scenes, std::move(glTFScene));
  return glTFSceneIndex;
}

void SceneConverter::_convertNode(fbxsdk::FbxNode &fbx_node_) {
  auto glTFNodeIndexX = _getNodeMap(fbx_node_);
  assert(glTFNodeIndexX);
  auto glTFNodeIndex = *glTFNodeIndexX;
  auto &glTFNode = _glTFBuilder.get(&fx::gltf::Document::nodes)[glTFNodeIndex];

  auto nodeName = _convertName(fbx_node_.GetName());
  glTFNode.name = nodeName;

  auto nChildren = fbx_node_.GetChildCount();
  for (auto iChild = 0; iChild < nChildren; ++iChild) {
    auto glTFNodeIndex = _getNodeMap(*fbx_node_.GetChild(iChild));
    assert(glTFNodeIndex);
    glTFNode.children.push_back(*glTFNodeIndex);
  }

  fbxsdk::FbxTransform::EInheritType inheritType;
  fbx_node_.GetTransformationInheritType(inheritType);
  if (inheritType == fbxsdk::FbxTransform::eInheritRrSs) {
    if (fbx_node_.GetParent() != nullptr) {
      _log(Logger::Level::warning,
           UnsupportedInheritTypeError{UnsupportedInheritTypeError::Type::RrSs,
                                       fbx_node_.GetName()});
    }
  } else if (inheritType == fbxsdk::FbxTransform::eInheritRrs) {
    _log(Logger::Level::warning,
         UnsupportedInheritTypeError{UnsupportedInheritTypeError::Type::Rrs,
                                     fbx_node_.GetName()});
  }

  if (auto fbxLocalTransform = fbx_node_.EvaluateLocalTransform();
      !fbxLocalTransform.IsIdentity()) {
    if (const auto fbxT = fbxLocalTransform.GetT(); !fbxT.IsZero(3)) {
      FbxVec3Spreader::spread(_applyUnitScaleFactorV3(fbxT),
                              glTFNode.translation.data());
    }
    if (const auto fbxR = fbxLocalTransform.GetQ();
        fbxR.Compare(fbxsdk::FbxQuaternion{})) {
      FbxQuatSpreader::spread(fbxR, glTFNode.rotation.data());
    }
    if (const auto fbxS = fbxLocalTransform.GetS();
        fbxS[0] != 1. || fbxS[1] != 1. || fbxS[2] != 1.) {
      FbxVec3Spreader::spread(fbxS, glTFNode.scale.data());
    }
  }

  FbxNodeDumpMeta nodeBumpData;
  nodeBumpData.glTFNodeIndex = glTFNodeIndex;

  std::vector<fbxsdk::FbxMesh *> fbxMeshes;
  for (auto nNodeAttributes = fbx_node_.GetNodeAttributeCount(),
            iNodeAttribute = 0;
       iNodeAttribute < nNodeAttributes; ++iNodeAttribute) {
    auto nodeAttribute = fbx_node_.GetNodeAttributeByIndex(iNodeAttribute);
    switch (const auto attributeType = nodeAttribute->GetAttributeType()) {
    case fbxsdk::FbxNodeAttribute::EType::eMesh:
      fbxMeshes.push_back(static_cast<fbxsdk::FbxMesh *>(nodeAttribute));
      break;
    case fbxsdk::FbxNodeAttribute::EType::eNull:
      // http://help.autodesk.com/view/FBX/2020/ENU/?guid=FBX_Developer_Help_nodes_and_scene_graph_fbx_node_attributes_html
      // > Some applications require a null node type in their scene graph.
      // > The FbxNull node attribute is used to define such a node type.
      // > Observe that an instance of FbxNull is not the same as the NULL
      // > value.
      break;
    case fbxsdk::FbxNodeAttribute::EType::eSkeleton: {
      const auto fbxSkeleton =
          static_cast<fbxsdk::FbxSkeleton *>(nodeAttribute);

      auto &skeletonExtra =
          glTFNode.extensionsAndExtras["extras"]["FBX-glTF-conv"]["skeleton"];

      if (!fbxSkeleton->GetSkeletonTypeIsSet()) {
        skeletonExtra = {};
      } else {
        const auto skeletonType = fbxSkeleton->GetSkeletonType();
        std::u8string_view skeletonTypeJson;
        switch (skeletonType) {
        case fbxsdk::FbxSkeleton::EType::eRoot:
          skeletonTypeJson = u8"Root";
          break;
        case fbxsdk::FbxSkeleton::EType::eLimb:
          skeletonTypeJson = u8"Limb";
          break;
        case fbxsdk::FbxSkeleton::EType::eLimbNode:
          skeletonTypeJson = u8"LimbNode";
          break;
        case fbxsdk::FbxSkeleton::EType::eEffector:
          skeletonTypeJson = u8"Effector";
          break;
        default:
          skeletonTypeJson = u8"";
          break;
        }
        if (!skeletonTypeJson.empty()) {
          skeletonExtra["skeletonType"] = forceTreatAsPlain(skeletonTypeJson);
        }
      }

      if (fbxSkeleton->GetLimbNodeColorIsSet()) {
        const auto limbColor = fbxSkeleton->GetLimbNodeColor();
        skeletonExtra["limbNodeColor"] = {{"r", limbColor.mRed},
                                          {"g", limbColor.mGreen},
                                          {"a", limbColor.mBlue},
                                          {"a", limbColor.mAlpha}};
      }
    } break;
    default:
      // TODO:
      // if (_options.verbose) {
      //  _log(Logger::Level::verbose,
      //       fmt::format(u8"Unhandled node attribute type: {}",
      //       attributeType));
      //}
      break;
    }
  }

  if (!fbxMeshes.empty()) {
    std::vector<fbxsdk::FbxMesh *> splittedMeshes;
    splittedMeshes.reserve(fbxMeshes.size());
    for (const auto mesh : fbxMeshes) {
      const auto splitted = _splitMeshesResult.equal_range(mesh);
      if (splitted.first != splitted.second) {
        std::transform(splitted.first, splitted.second, std::back_inserter(splittedMeshes), [](auto kv_) { return std::get<1>(kv_); });
      } else {
        splittedMeshes.push_back(mesh);
      }
    }

    const auto convertMeshResult =
        _convertNodeMeshes(nodeBumpData, splittedMeshes, fbx_node_);
    if (convertMeshResult) {
      glTFNode.mesh = convertMeshResult->glTFMeshIndex;
      if (convertMeshResult->glTFSkinIndex) {
        glTFNode.skin = *convertMeshResult->glTFSkinIndex;
      }
    }
  }

  _nodeDumpMetaMap.emplace(&fbx_node_, nodeBumpData);
}

std::string SceneConverter::_getName(fbxsdk::FbxNode &fbx_node_) {
  return fbx_string_to_utf8_checked(fbx_node_.GetName());
}
} // namespace bee