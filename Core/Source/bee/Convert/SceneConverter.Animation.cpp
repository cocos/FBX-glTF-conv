
#include <fmt/format.h>
#include <bee/Convert/DirectSpreader.h>
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/Spreader.h>

namespace bee {
void SceneConverter::_convertAnimation(fbxsdk::FbxScene &fbx_scene_) {
  const auto nAnimStacks = fbx_scene_.GetSrcObjectCount<fbxsdk::FbxAnimStack>();
  for (std::remove_const_t<decltype(nAnimStacks)> iAnimStack = 0;
       iAnimStack < nAnimStacks; ++iAnimStack) {
    const auto animStack =
        fbx_scene_.GetSrcObject<fbxsdk::FbxAnimStack>(iAnimStack);
    const auto nAnimLayers = animStack->GetMemberCount<fbxsdk::FbxAnimLayer>();
    if (!nAnimLayers) {
      continue;
    }

    const auto timeSpan = _getAnimStackTimeSpan(*animStack);
    if (timeSpan.GetDuration() == 0) {
      continue;
    }
    const auto usedAnimationTimeMode = _animationTimeMode;
    const auto firstFrame =
        timeSpan.GetStart().GetFrameCount(usedAnimationTimeMode);
    const auto lastFrame =
        timeSpan.GetStop().GetFrameCount(usedAnimationTimeMode);
    assert(lastFrame >= firstFrame);
    AnimRange animRange{_animationTimeMode, firstFrame, lastFrame};

    fx::gltf::Animation glTFAnimation;
    const auto animName = _convertName(animStack->GetName());
    glTFAnimation.name = animName;
    fbx_scene_.SetCurrentAnimationStack(animStack);
    for (std::remove_const_t<decltype(nAnimLayers)> iAnimLayer = 0;
         iAnimLayer < nAnimLayers; ++iAnimLayer) {
      const auto animLayer =
          animStack->GetMember<fbxsdk::FbxAnimLayer>(iAnimLayer);
      _convertAnimationLayer(glTFAnimation, *animLayer, fbx_scene_, animRange);
    }
    if (!glTFAnimation.samplers.empty()) {
      _glTFBuilder.add(&fx::gltf::Document::animations,
                       std::move(glTFAnimation));
    }
  }
}

fbxsdk::FbxTimeSpan SceneConverter::_getAnimStackTimeSpan(
    const fbxsdk::FbxAnimStack &fbx_anim_stack_) {
  const auto nAnimLayers =
      fbx_anim_stack_.GetMemberCount<fbxsdk::FbxAnimLayer>();
  if (!nAnimLayers) {
    return {};
  }
  fbxsdk::FbxTimeSpan animTimeSpan;
  for (std::remove_const_t<decltype(nAnimLayers)> iAnimLayer = 0;
       iAnimLayer < nAnimLayers; ++iAnimLayer) {
    const auto animLayer =
        fbx_anim_stack_.GetMember<fbxsdk::FbxAnimLayer>(iAnimLayer);
    const auto nCurveNodes =
        animLayer->GetMemberCount<fbxsdk::FbxAnimCurveNode>();
    for (std::remove_const_t<decltype(nCurveNodes)> iCurveNode = 0;
         iCurveNode < nCurveNodes; ++iCurveNode) {
      const auto curveNode =
          animLayer->GetMember<fbxsdk::FbxAnimCurveNode>(iCurveNode);
      if (!curveNode->IsAnimated()) {
        continue;
      }
      fbxsdk::FbxTimeSpan curveNodeInterval;
      if (!curveNode->GetAnimationInterval(curveNodeInterval)) {
        continue;
      }
      animTimeSpan.UnionAssignment(curveNodeInterval);
    }
  }
  return animTimeSpan;
}

void SceneConverter::_convertAnimationLayer(
    fx::gltf::Animation &glTF_animation_,
    fbxsdk::FbxAnimLayer &fbx_anim_layer_,
    fbxsdk::FbxScene &fbx_scene_,
    const AnimRange &anim_range_) {
  const auto nNodes = fbx_scene_.GetNodeCount();
  for (std::remove_const_t<decltype(nNodes)> iNode = 0; iNode < nNodes;
       ++iNode) {
    auto fbxNode = fbx_scene_.GetNode(iNode);
    _convertAnimationLayer(glTF_animation_, fbx_anim_layer_, *fbxNode,
                           anim_range_);
  }
}

void SceneConverter::_convertAnimationLayer(
    fx::gltf::Animation &glTF_animation_,
    fbxsdk::FbxAnimLayer &fbx_anim_layer_,
    fbxsdk::FbxNode &fbx_node_,
    const AnimRange &anim_range_) {
  if (_options.export_trs_animation) {
    _extractTrsAnimation(glTF_animation_, fbx_anim_layer_, fbx_node_,
                         anim_range_);
  }

  if (_options.export_blend_shape_animation) {
    _extractWeightsAnimation(glTF_animation_, fbx_anim_layer_, fbx_node_,
                             anim_range_);
  }
}

void SceneConverter::_extractWeightsAnimation(
    fx::gltf::Animation &glTF_animation_,
    fbxsdk::FbxAnimLayer &fbx_anim_layer_,
    fbxsdk::FbxNode &fbx_node_,
    const AnimRange &anim_range_) {
  auto rNodeBumpMeta = _nodeDumpMetaMap.find(&fbx_node_);
  if (rNodeBumpMeta == _nodeDumpMetaMap.end()) {
    return;
  }

  const auto &nodeBumpMeta = rNodeBumpMeta->second;
  if (!nodeBumpMeta.meshes) {
    return;
  }

  auto &blendShapeMeta = nodeBumpMeta.meshes->blendShapeMeta;
  if (!blendShapeMeta) {
    return;
  }

  auto &fbxMeshes = nodeBumpMeta.meshes->meshes;
  if (!fbxMeshes.empty()) {
    std::vector<MorphAnimation> morphAnimations{fbxMeshes.size()};
    for (decltype(fbxMeshes.size()) iMesh = 0; iMesh < fbxMeshes.size();
         ++iMesh) {
      const auto &blendShapeData = blendShapeMeta->blendShapeDatas[iMesh];
      morphAnimations[iMesh] = _extractWeightsAnimation(
          fbx_anim_layer_, fbx_node_, *fbxMeshes[iMesh], blendShapeData,
          anim_range_);
    }

    if (const auto &first = morphAnimations.front(); std::all_of(
            std::next(morphAnimations.begin()), morphAnimations.end(),
            [&first](const MorphAnimation &anim_) {
              return anim_.times == first.times && anim_.values == first.values;
            })) {
      _writeMorphAnimtion(glTF_animation_, first, nodeBumpMeta.glTFNodeIndex,
                          fbx_node_);
    } else {
      _warn(fmt::format("Sub-meshes use different morph animation. We can't "
                        "handle that case."));
    }
  }
}

void SceneConverter::_writeMorphAnimtion(fx::gltf::Animation &glTF_animation_,
                                         const MorphAnimation &morph_animtion_,
                                         std::uint32_t glTF_node_index_,
                                         const fbxsdk::FbxNode &fbx_node_) {
  auto timeAccessorIndex = _glTFBuilder.createAccessor<
      fx::gltf::Accessor::Type::Scalar,
      fx::gltf::Accessor::ComponentType::Float,
      DirectSpreader<decltype(morph_animtion_.times)::value_type>>(
      std::span{morph_animtion_.times}, 0, 0, true);
  _glTFBuilder.get(&fx::gltf::Document::accessors)[timeAccessorIndex].name =
      fmt::format("{}/weights/Input", fbx_node_.GetName());

  auto weightsAccessorIndex = _glTFBuilder.createAccessor<
      fx::gltf::Accessor::Type::Scalar,
      fx::gltf::Accessor::ComponentType::Float,
      DirectSpreader<decltype(morph_animtion_.values)::value_type>>(
      morph_animtion_.values, 0, 0);
  _glTFBuilder.get(&fx::gltf::Document::accessors)[weightsAccessorIndex].name =
      fmt::format("{}/weights/Output", fbx_node_.GetName());

  fx::gltf::Animation::Sampler sampler;
  sampler.input = timeAccessorIndex;
  sampler.output = weightsAccessorIndex;
  auto samplerIndex = glTF_animation_.samplers.size();
  glTF_animation_.samplers.emplace_back(std::move(sampler));
  fx::gltf::Animation::Channel channel;
  channel.target.node = glTF_node_index_;
  channel.target.path = "weights";
  channel.sampler = static_cast<std::int32_t>(samplerIndex);
  glTF_animation_.channels.push_back(channel);
}

SceneConverter::MorphAnimation SceneConverter::_extractWeightsAnimation(
    fbxsdk::FbxAnimLayer &fbx_anim_layer_,
    const fbxsdk::FbxNode &fbx_node_,
    fbxsdk::FbxMesh &fbx_mesh_,
    const FbxBlendShapeData &blend_shape_data_,
    const AnimRange &anim_range_) {
  using WeightType = decltype(MorphAnimation::values)::value_type;

  MorphAnimation morphAnimation;

  using TargetWeightsCount = std::size_t;
  auto nTargetWeights = std::accumulate(
      blend_shape_data_.channels.begin(), blend_shape_data_.channels.end(),
      static_cast<TargetWeightsCount>(0),
      [](TargetWeightsCount sum_, const auto &channel_) {
        return sum_ + channel_.targetShapes.size();
      });

  const auto nFrames = static_cast<decltype(MorphAnimation::times)::size_type>(
      anim_range_.frames_count());
  const auto timeMode = anim_range_.timeMode;
  morphAnimation.times.resize(nFrames, 0.0);
  morphAnimation.values.resize(nTargetWeights * nFrames);

  auto extractFrame =
      [](decltype(MorphAnimation::values)::iterator out_weights_,
         fbxsdk::FbxTime time_, fbxsdk::FbxAnimCurve *shape_channel_,
         const decltype(
             FbxBlendShapeData::Channel::targetShapes) &target_shapes_) {
        if (target_shapes_.empty()) {
          return;
        }

        constexpr auto zeroWeight = static_cast<WeightType>(0.0);
        constexpr fbxsdk::FbxDouble defaultWeight = 0;

        const auto iFrameWeightsBeg = out_weights_;
        const auto iFrameWeightsEnd = iFrameWeightsBeg + target_shapes_.size();

        const auto animWeight =
            shape_channel_ ? shape_channel_->Evaluate(time_) : defaultWeight;

        // The target shape 'fullWeight' values are
        // a strictly ascending list of floats (between 0 and 100), forming a
        // sequence of intervals.
        assert(std::is_sorted(target_shapes_.begin(), target_shapes_.end(),
                              [](const auto &lhs_, const auto &rhs_) {
                                return std::get<1>(lhs_) < std::get<1>(rhs_);
                              }));
        const auto firstNotLessThan = std::find_if(
            target_shapes_.begin(), target_shapes_.end(),
            [animWeight](
                const std::decay_t<decltype(target_shapes_)>::value_type
                    &target_) {
              const auto &[targetShape, targetWeightThreshold] = target_;
              return targetWeightThreshold >= animWeight;
            });

        if (firstNotLessThan == target_shapes_.begin()) {
          const auto firstThreshold = std::get<1>(target_shapes_.front());
          *iFrameWeightsBeg = animWeight / firstThreshold;
        } else if (firstNotLessThan != target_shapes_.end()) {
          const auto iRight = firstNotLessThan - target_shapes_.begin();
          assert(iRight);
          const auto iLeft = iRight - 1;
          const auto leftWeight = std::get<1>(target_shapes_[iLeft]);
          const auto rightWeight = std::get<1>(target_shapes_[iRight]);
          const auto ratio =
              (animWeight - leftWeight) / (rightWeight - leftWeight);
          iFrameWeightsBeg[iLeft] = static_cast<WeightType>(ratio);
          iFrameWeightsBeg[iRight] = static_cast<WeightType>(1.0 - ratio);
        }
      };

  for (decltype(morphAnimation.times.size()) iFrame = 0;
       iFrame < morphAnimation.times.size(); ++iFrame) {
    const auto fbxFrame = anim_range_.firstFrame + iFrame;
    fbxsdk::FbxTime time;
    time.SetFrame(fbxFrame, timeMode);

    morphAnimation.times[iFrame] = time.GetSecondDouble();

    TargetWeightsCount offset = 0;
    for (const auto &[blendShapeIndex, blendShapeChannelIndex, name,
                      deformPercent, targetShapes] :
         blend_shape_data_.channels) {
      auto shapeChannel = fbx_mesh_.GetShapeChannel(
          blendShapeIndex, blendShapeChannelIndex, &fbx_anim_layer_);
      const auto outWeights =
          morphAnimation.values.begin() + nTargetWeights * iFrame + offset;
      extractFrame(outWeights, time, shapeChannel, targetShapes);
      offset += targetShapes.size();
    }
  }

  return morphAnimation;
}

void SceneConverter::_extractTrsAnimation(fx::gltf::Animation &glTF_animation_,
                                          fbxsdk::FbxAnimLayer &fbx_anim_layer_,
                                          fbxsdk::FbxNode &fbx_node_,
                                          const AnimRange &anim_range_) {
  const auto glTFNodeIndex = _getNodeMap(fbx_node_);
  if (!glTFNodeIndex) {
    return;
  }

  const auto isTranslationAnimated =
      fbx_node_.LclTranslation.IsAnimated(&fbx_anim_layer_);
  const auto isRotationAnimated =
      fbx_node_.LclRotation.IsAnimated(&fbx_anim_layer_);
  const auto isScaleAnimated =
      fbx_node_.LclScaling.IsAnimated(&fbx_anim_layer_);
  if (!isTranslationAnimated && !isRotationAnimated && !isScaleAnimated) {
    return;
  }

  const auto nFrames = static_cast<decltype(MorphAnimation::times)::size_type>(
      anim_range_.lastFrame - anim_range_.firstFrame);
  std::vector<double> times;
  std::vector<fbxsdk::FbxVector4> translations;
  std::vector<fbxsdk::FbxQuaternion> rotations;
  std::vector<fbxsdk::FbxVector4> scales;

  for (std::remove_const_t<decltype(nFrames)> iFrame = 0; iFrame < nFrames;
       ++iFrame) {
    const auto fbxFrame = anim_range_.firstFrame + iFrame;
    fbxsdk::FbxTime time;
    time.SetFrame(fbxFrame, anim_range_.timeMode);

    const auto localTransform = fbx_node_.EvaluateLocalTransform(time);
    bool mayBeOptOut = false;

    fbxsdk::FbxVector4 translation;
    if (isTranslationAnimated) {
      translation = localTransform.GetT();
    }
    fbxsdk::FbxQuaternion rotation;
    if (isRotationAnimated) {
      rotation = localTransform.GetQ();
      rotation.Normalize();
    }
    fbxsdk::FbxVector4 scale;
    if (isScaleAnimated) {
      scale = localTransform.GetS();
    }

    if (!mayBeOptOut) {
      times.push_back(time.GetSecondDouble());
      if (isTranslationAnimated) {
        translations.push_back(translation);
      }
      if (isRotationAnimated) {
        rotations.push_back(rotation);
      }
      if (isScaleAnimated) {
        scales.push_back(scale);
      }
    }
  }

  auto timeAccessorIndex =
      _glTFBuilder.createAccessor<fx::gltf::Accessor::Type::Scalar,
                                  fx::gltf::Accessor::ComponentType::Float,
                                  DirectSpreader<decltype(times)::value_type>>(
          times, 0, 0, true);
  _glTFBuilder.get(&fx::gltf::Document::accessors)[timeAccessorIndex].name =
      fmt::format("{}/Trs/Input", fbx_node_.GetName());
  auto addChannel = [timeAccessorIndex, &glTF_animation_, glTFNodeIndex, this,
                     &fbx_node_](std::string_view path_,
                                 std::uint32_t value_accessor_index_) {
    _glTFBuilder.get(&fx::gltf::Document::accessors)[value_accessor_index_]
        .name = fmt::format("{}/{}/Output", fbx_node_.GetName(), path_);
    fx::gltf::Animation::Sampler sampler;
    sampler.input = timeAccessorIndex;
    sampler.output = value_accessor_index_;
    auto samplerIndex = glTF_animation_.samplers.size();
    glTF_animation_.samplers.emplace_back(std::move(sampler));
    fx::gltf::Animation::Channel channel;
    channel.target.node = *glTFNodeIndex;
    channel.target.path = path_;
    channel.sampler = static_cast<std::int32_t>(samplerIndex);
    glTF_animation_.channels.push_back(channel);
  };

  using ComponentType =
      GLTFComponentTypeStorage<fx::gltf::Accessor::ComponentType::Float>;
  if (isTranslationAnimated) {
    auto valueAccessorIndex =
        _glTFBuilder.createAccessor<fx::gltf::Accessor::Type::Vec3,
                                    fx::gltf::Accessor::ComponentType::Float,
                                    FbxVec3Spreader>(translations, 0, 0);
    addChannel("translation", valueAccessorIndex);
  }
  if (isRotationAnimated) {
    auto valueAccessorIndex =
        _glTFBuilder.createAccessor<fx::gltf::Accessor::Type::Vec4,
                                    fx::gltf::Accessor::ComponentType::Float,
                                    FbxQuatSpreader>(rotations, 0, 0);
    addChannel("rotation", valueAccessorIndex);
  }
  if (isScaleAnimated) {
    auto valueAccessorIndex =
        _glTFBuilder.createAccessor<fx::gltf::Accessor::Type::Vec3,
                                    fx::gltf::Accessor::ComponentType::Float,
                                    FbxVec3Spreader>(scales, 0, 0);
    addChannel("scale", valueAccessorIndex);
  }
}
} // namespace bee