
#include <bee/Convert/ConvertError.h>
#include <bee/Convert/SceneConverter.h>
#include <fmt/format.h>

namespace bee {
/// <summary>
/// glTF does not allow sub-meshes have different number of targets.
/// </summary>
class InconsistentTargetsCountError
    : public NodeError<InconsistentTargetsCountError> {
public:
  constexpr static inline std::u8string_view code =
      u8"inconsistent_target_count";

  using NodeError::NodeError;
};

void to_json(nlohmann::json &j_, const InconsistentTargetsCountError &error_) {
  to_json(j_, static_cast<const NodeError<InconsistentTargetsCountError> &>(
                  error_));
}

std::optional<SceneConverter::FbxBlendShapeData>
SceneConverter::_extractdBlendShapeData(const fbxsdk::FbxMesh &fbx_mesh_) {
  FbxBlendShapeData blendShapeData;

  const auto nBlendShape = fbx_mesh_.GetDeformerCount(
      fbxsdk::FbxDeformer::EDeformerType::eBlendShape);

  for (std::remove_const_t<decltype(nBlendShape)> iBlendShape = 0;
       iBlendShape < nBlendShape; ++iBlendShape) {
    const auto fbxBlendShape =
        static_cast<fbxsdk::FbxBlendShape *>(fbx_mesh_.GetDeformer(
            iBlendShape, fbxsdk::FbxDeformer::EDeformerType::eBlendShape));
    const auto nChannels = fbxBlendShape->GetBlendShapeChannelCount();
    for (std::remove_const_t<decltype(nChannels)> iChannel = 0;
         iChannel < nChannels; ++iChannel) {
      const auto blendShapeChannel =
          fbxBlendShape->GetBlendShapeChannel(iChannel);
      auto fullWeights = blendShapeChannel->GetTargetShapeFullWeights();
      if (const auto nTargetShapes = blendShapeChannel->GetTargetShapeCount()) {
        decltype(FbxBlendShapeData::Channel::targetShapes) targetShapes(
            nTargetShapes);
        for (std::remove_const_t<decltype(nTargetShapes)> iTargetShape = 0;
             iTargetShape < nTargetShapes; ++iTargetShape) {
          auto targetShape = blendShapeChannel->GetTargetShape(iTargetShape);
          targetShapes[iTargetShape] = {targetShape, fullWeights[iTargetShape]};
        }
        blendShapeData.channels.push_back(FbxBlendShapeData::Channel{
            iBlendShape, iChannel, _convertName(blendShapeChannel->GetName()),
            blendShapeChannel->DeformPercent.Get(), std::move(targetShapes)});
      }
    }
  }

  if (blendShapeData.channels.empty()) {
    return {};
  }

  return blendShapeData;
}

std::optional<SceneConverter::FbxNodeMeshesBumpMeta::BlendShapeDumpMeta>
SceneConverter::_extractNodeMeshesBlendShape(
    const std::vector<fbxsdk::FbxMesh *> &fbx_meshes_) {
  if (fbx_meshes_.empty()) {
    return {};
  }

  std::vector<std::optional<FbxBlendShapeData>> blendShapeDatas;
  blendShapeDatas.reserve(fbx_meshes_.size());
  std::transform(fbx_meshes_.begin(), fbx_meshes_.end(),
                 std::back_inserter(blendShapeDatas),
                 [this](fbxsdk::FbxMesh *fbx_mesh_) {
                   return _extractdBlendShapeData(*fbx_mesh_);
                 });

  const auto &firstBlendShapeData = blendShapeDatas.front();

  const auto hasSameStruct =
      [&](const std::optional<FbxBlendShapeData> &blend_shape_data_) {
        if (blend_shape_data_.has_value() != firstBlendShapeData.has_value()) {
          return false;
        }
        if (!firstBlendShapeData) {
          return true;
        }
        if (firstBlendShapeData->channels.size() !=
            blend_shape_data_->channels.size()) {
          return false;
        }
        if (!std::equal(firstBlendShapeData->channels.begin(),
                        firstBlendShapeData->channels.end(),
                        blend_shape_data_->channels.begin(),
                        blend_shape_data_->channels.end(),
                        [](const FbxBlendShapeData::Channel &lhs_,
                           const FbxBlendShapeData::Channel &rhs_) {
                          return lhs_.name == rhs_.name &&
                                 lhs_.targetShapes.size() ==
                                     rhs_.targetShapes.size();
                        })) {
          return false;
        }
        return true;
      };

  if (!std::all_of(std::next(blendShapeDatas.begin()), blendShapeDatas.end(),
                   hasSameStruct)) {
    _log(Logger::Level::warning,
         InconsistentTargetsCountError{
             fbx_meshes_.front()->GetNode()->GetName()});
  }

  if (!firstBlendShapeData) {
    return {};
  }

  FbxNodeMeshesBumpMeta::BlendShapeDumpMeta myMeta;
  myMeta.blendShapeDatas.reserve(blendShapeDatas.size());
  std::transform(blendShapeDatas.begin(), blendShapeDatas.end(),
                 std::back_inserter(myMeta.blendShapeDatas),
                 [](auto &v_) { return std::move(*v_); });

  return myMeta;
}
} // namespace bee