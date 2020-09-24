
#include <bee/Convert/ConvertError.h>
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/Spreader.h>
#include <fmt/format.h>

namespace bee {
/// <summary>
/// Node {} uses unsupported transform inheritance type '{}'.
/// </summary>
class InheritTypeError : public NodeError {
public:
  enum class Type {
    RrSs,
    Rrs,
  };

  InheritTypeError(Type type_, std::string_view node_)
      : _type(type_), NodeError(node_) {
  }

  Type type() const {
    return _type;
  }

private:
  Type _type;
};

void to_json(nlohmann::json &j_, InheritTypeError::Type inherit_type_) {
  j_ = inherit_type_ == InheritTypeError::Type::Rrs ? "Rrs" : "RrSs";
}

void to_json(nlohmann::json &j_, const InheritTypeError &error_) {
  j_ = nlohmann::json{{"node", error_.node()}, {"type", error_.type()}};
}

SceneConverter::SceneConverter(fbxsdk::FbxManager &fbx_manager_,
                               fbxsdk::FbxScene &fbx_scene_,
                               const ConvertOptions &options_,
                               std::u8string_view fbx_file_name_,
                               GLTFBuilder &glTF_builder_)
    : _glTFBuilder(glTF_builder_), _fbxManager(fbx_manager_),
      _fbxScene(fbx_scene_), _options(options_), _fbxFileName(fbx_file_name_),
      _fbxGeometryConverter(&fbx_manager_) {
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
    message_["level"] = level_;
    (*_options.logger)(std::move(message_));
  }
}

fbxsdk::FbxGeometryConverter &SceneConverter::_getGeometryConverter() {
  return _fbxGeometryConverter;
}

void SceneConverter::_prepareScene() {
  // Convert axis system
  fbxsdk::FbxAxisSystem::OpenGL.ConvertScene(&_fbxScene);

  // Convert system unit
  if (_fbxScene.GetGlobalSettings().GetSystemUnit() !=
      fbxsdk::FbxSystemUnit::m) {
    fbxsdk::FbxSystemUnit::ConversionOptions conversionOptions;
    conversionOptions.mConvertRrsNodes = false;
    conversionOptions.mConvertLimits = true;
    conversionOptions.mConvertClusters = true;
    conversionOptions.mConvertLightIntensity = true;
    conversionOptions.mConvertPhotometricLProperties = true;
    conversionOptions.mConvertCameraClipPlanes = true;
    fbxsdk::FbxSystemUnit::m.ConvertScene(&_fbxScene, conversionOptions);
  }

  // Trianglute the whole scene
  _fbxGeometryConverter.Triangulate(&_fbxScene, true);

  // Split meshes per material
  _fbxGeometryConverter.SplitMeshesPerMaterial(&_fbxScene, true);
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

std::string SceneConverter::_convertFileName(const char *fbx_file_name_) {
  return fbx_file_name_;
}

GLTFBuilder::XXIndex
SceneConverter::_convertScene(fbxsdk::FbxScene &fbx_scene_) {
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
           InheritTypeError{InheritTypeError::Type::RrSs, fbx_node_.GetName()});
    }
  } else if (inheritType == fbxsdk::FbxTransform::eInheritRrs) {
    _log(Logger::Level::warning,
         InheritTypeError{InheritTypeError::Type::Rrs, fbx_node_.GetName()});
  }

  if (auto fbxLocalTransform = fbx_node_.EvaluateLocalTransform();
      !fbxLocalTransform.IsIdentity()) {
    if (auto fbxT = fbxLocalTransform.GetT(); !fbxT.IsZero(3)) {
      FbxVec3Spreader::spread(fbxT, glTFNode.translation.data());
    }
    if (auto fbxR = fbxLocalTransform.GetQ();
        fbxR.Compare(fbxsdk::FbxQuaternion{})) {
      FbxQuatSpreader::spread(fbxR, glTFNode.rotation.data());
    }
    if (auto fbxS = fbxLocalTransform.GetS();
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
    switch (auto attributeType = nodeAttribute->GetAttributeType()) {
    case fbxsdk::FbxNodeAttribute::EType::eMesh:
      fbxMeshes.push_back(static_cast<fbxsdk::FbxMesh *>(nodeAttribute));
      break;
    default:
      break;
    }
  }

  if (!fbxMeshes.empty()) {
    auto convertMeshResult =
        _convertNodeMeshes(nodeBumpData, fbxMeshes, fbx_node_);
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
  return fbx_node_.GetName();
}
} // namespace bee