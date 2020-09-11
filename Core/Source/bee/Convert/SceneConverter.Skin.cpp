
#include <fmt/format.h>
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/Spreader.h>

namespace bee {
struct SceneConverter::MeshSkinData::Bone::IBMSpreader {
  using type = Bone;

  constexpr static auto size = FbxAMatrixSpreader::size;

  template <typename TargetTy_>
  static void spread(const type &in_, TargetTy_ *out_) {
    return FbxAMatrixSpreader::spread(in_.inverseBindMatrix, out_);
  }
};

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
std::optional<SceneConverter::NodeMeshesSkinData>
SceneConverter::_extractNodeMeshesSkinData(
    const std::vector<fbxsdk::FbxMesh *> &fbx_meshes_) {
  NodeMeshesSkinData nodeMeshesSkinData;
  nodeMeshesSkinData.meshChannels.resize(fbx_meshes_.size());

  auto &newBones = nodeMeshesSkinData.bones;
  auto &meshChannels = nodeMeshesSkinData.meshChannels;

  decltype(fbx_meshes_.size()) nSkinnedMeshes = 0;
  for (decltype(fbx_meshes_.size()) iFbxMesh = 0; iFbxMesh < fbx_meshes_.size();
       ++iFbxMesh) {
    const auto &fbxMesh = *fbx_meshes_[iFbxMesh];
    auto meshSkinData = _extractSkinData(fbxMesh);
    if (!meshSkinData || meshSkinData->bones.empty()) {
      continue;
    }
    ++nSkinnedMeshes;
    auto &partBones = meshSkinData->bones;
    auto &partChannels = meshSkinData->channels;
    if (nSkinnedMeshes == 1) {
      newBones = std::move(partBones);
      meshChannels[iFbxMesh] = std::move(partChannels);
    } else {
      // Merge into new skin
      for (decltype(partBones.size()) iBone = 0; iBone < partBones.size();
           ++iBone) {
        const auto &partBone = partBones[iBone];
        auto rNewBone =
            std::find_if(newBones.begin(), newBones.end(),
                         [&partBone](const MeshSkinData::Bone &new_bone_) {
                           return partBone.glTFNode == new_bone_.glTFNode;
                         });
        NeutralVertexJointComponent newIndex = 0;
        if (rNewBone != newBones.end()) {
          newIndex =
              static_cast<decltype(newIndex)>(rNewBone - newBones.begin());
          if (rNewBone->inverseBindMatrix != partBone.inverseBindMatrix) {
            _warn(fmt::format(
                "Joint {} has different inverse bind matrices "
                "for different meshes.",
                _glTFBuilder.get(&fx::gltf::Document::nodes)[partBone.glTFNode]
                    .name));
          }
        } else {
          newIndex = static_cast<NeutralVertexJointComponent>(newBones.size());
          newBones.emplace_back(partBone);
        }
        for (auto &partChannel : partChannels) {
          for (auto &i : partChannel.joints) {
            if (i == iBone) {
              i = newIndex;
            }
          }
        }
      }
      // Remap joint indices in channel to new
      meshChannels[iFbxMesh] = std::move(partChannels);
    }
  }

  if (nodeMeshesSkinData.bones.empty()) {
    return {};
  } else {
    return nodeMeshesSkinData;
  }
}

std::optional<SceneConverter::MeshSkinData>
SceneConverter::_extractSkinData(const fbxsdk::FbxMesh &fbx_mesh_) {
  using ResultJointType =
      decltype(MeshSkinData::InfluenceChannel::joints)::value_type;
  using ResultWeightType =
      decltype(MeshSkinData::InfluenceChannel::weights)::value_type;

  MeshSkinData skinData;
  auto &[skinName, skinJoints, skinChannels] = skinData;

  const auto nControlPoints = fbx_mesh_.GetControlPointsCount();
  std::vector<decltype(MeshSkinData::channels)::size_type> channelsCount(
      nControlPoints);

  auto allocateOneChannel = [&skinData, nControlPoints]() {
    auto &channel = skinData.channels.emplace_back();
    channel.joints.resize(nControlPoints);
    channel.weights.resize(nControlPoints);
  };

  const auto nSkinDeformers =
      fbx_mesh_.GetDeformerCount(fbxsdk::FbxDeformer::EDeformerType::eSkin);
  for (std::remove_const_t<decltype(nSkinDeformers)> iSkinDeformer = 0;
       iSkinDeformer < nSkinDeformers; ++iSkinDeformer) {
    const auto skinDeformer =
        static_cast<fbxsdk::FbxSkin *>(fbx_mesh_.GetDeformer(
            iSkinDeformer, fbxsdk::FbxDeformer::EDeformerType::eSkin));

    skinName = _convertName(skinDeformer->GetName());

    const auto nClusters = skinDeformer->GetClusterCount();
    for (std::remove_const_t<decltype(nClusters)> iCluster = 0;
         iCluster < nClusters; ++iCluster) {
      const auto cluster = skinDeformer->GetCluster(iCluster);

      const auto jointNode = cluster->GetLink();
      const auto glTFNodeIndex = _getNodeMap(*jointNode);
      if (!glTFNodeIndex) {
        // TODO: may be we should do some work here??
        _warn(fmt::format("The joint node \"{}\" is used for skinning but "
                          "missed in scene graph.It will be ignored.",
                          jointNode->GetName()));
        continue;
      }

      switch (const auto linkMode = cluster->GetLinkMode()) {
      case fbxsdk::FbxCluster::eAdditive:
        _warn(fmt::format("Unsupported cluster mode \"additive\" [Mesh: {}; "
                          "ClusterLink: {}]",
                          fbx_mesh_.GetName(), jointNode->GetName()));
        break;
      case fbxsdk::FbxCluster::eNormalize:
      case fbxsdk::FbxCluster::eTotalOne:
      default:
        break;
      }

      // Index this node to joint array
      auto rJointIndex =
          std::find_if(skinJoints.begin(), skinJoints.end(),
                       [&glTFNodeIndex](const MeshSkinData::Bone &joint_) {
                         return joint_.glTFNode == *glTFNodeIndex;
                       });
      if (rJointIndex == skinJoints.end()) {
        fbxsdk::FbxAMatrix transformMatrix;
        cluster->GetTransformMatrix(transformMatrix);
        fbxsdk::FbxAMatrix transformLinkMatrix;
        cluster->GetTransformLinkMatrix(transformLinkMatrix);
        // http://blog.csdn.net/bugrunner/article/details/7232291
        // http://help.autodesk.com/view/FBX/2017/ENU/?guid=__cpp_ref__view_scene_2_draw_scene_8cxx_example_html
        const auto inverseBindMatrix =
            transformLinkMatrix.Inverse() * transformMatrix;

        skinJoints.emplace_back(
            MeshSkinData::Bone{*glTFNodeIndex, inverseBindMatrix});
        rJointIndex = std::prev(skinJoints.end());
      }
      const auto jointId =
          static_cast<std::uint32_t>(rJointIndex - skinJoints.begin());

      const auto nControlPointIndices = cluster->GetControlPointIndicesCount();
      const auto controlPointIndices = cluster->GetControlPointIndices();
      const auto controlPointWeights = cluster->GetControlPointWeights();
      for (std::remove_const_t<decltype(nControlPointIndices)>
               iControlPointIndex = 0;
           iControlPointIndex < nControlPointIndices; ++iControlPointIndex) {
        const auto controlPointIndex = controlPointIndices[iControlPointIndex];
        auto &nChannels = channelsCount[controlPointIndex];
        assert(nChannels <= skinData.channels.size());
        if (nChannels == skinData.channels.size()) {
          allocateOneChannel();
        }
        auto &[joints, weights] = skinData.channels[nChannels];
        joints[controlPointIndex] = jointId;
        weights[controlPointIndex] = static_cast<NeutralVertexWeightComponent>(
            controlPointWeights[iControlPointIndex]);
        ++nChannels;
      }
    }
  }

  if (skinJoints.empty()) {
    return {};
  }

  // Normalize weights
  for (std::remove_const_t<decltype(nControlPoints)> iControlPoint = 0;
       iControlPoint < nControlPoints; ++iControlPoint) {
    const auto nChannels = channelsCount[iControlPoint];
    if (nChannels > 0) {
      auto sum = static_cast<ResultWeightType>(0.0);
      for (std::remove_const_t<decltype(nChannels)> iChannel = 0;
           iChannel < nChannels; ++iChannel) {
        sum += skinData.channels[iChannel].weights[iControlPoint];
      }
      if (sum != 0.0) {
        for (std::remove_const_t<decltype(nChannels)> iChannel = 0;
             iChannel < nChannels; ++iChannel) {
          skinData.channels[iChannel].weights[iControlPoint] /= sum;
        }
      }
    }
  }

  return skinData;
}

std::uint32_t
SceneConverter::_createGLTFSkin(const NodeMeshesSkinData &skin_data_) {
  fx::gltf::Skin glTFSkin;
  glTFSkin.joints.resize(skin_data_.bones.size());
  std::transform(
      skin_data_.bones.begin(), skin_data_.bones.end(), glTFSkin.joints.begin(),
      [](const MeshSkinData::Bone &bone_) { return bone_.glTFNode; });

  const auto ibmAccessorIndex =
      _glTFBuilder.createAccessor<fx::gltf::Accessor::Type::Mat4,
                                  fx::gltf::Accessor::ComponentType::Float,
                                  MeshSkinData::Bone::IBMSpreader>(
          skin_data_.bones, 0, 0);
  auto &ibmAccessor =
      _glTFBuilder.get(&fx::gltf::Document::accessors)[ibmAccessorIndex];
  ibmAccessor.name = fmt::format("{}/InverseBindMatrices", skin_data_.name);
  glTFSkin.inverseBindMatrices = ibmAccessorIndex;

  const auto glTFSkinIndex =
      _glTFBuilder.add(&fx::gltf::Document::skins, std::move(glTFSkin));
  return glTFSkinIndex;
}
} // namespace bee