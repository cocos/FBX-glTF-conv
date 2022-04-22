
#include <bee/Convert/ConvertError.h>
#include <bee/Convert/DirectSpreader.h>
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/Spreader.h>
#include <fmt/format.h>

constexpr static auto defaultEplislon = static_cast<fbxsdk::FbxDouble>(1e-6);

bool isApproximatelyEqual(fbxsdk::FbxDouble from_,
                          fbxsdk::FbxDouble to_,
                          fbxsdk::FbxDouble eplislon_ = defaultEplislon) {
  return std::abs(from_ - to_) < eplislon_;
}

template <int Size_>
bool isApproximatelyEqual(const fbxsdk::FbxDouble *from_,
                          const fbxsdk::FbxDouble *to_,
                          fbxsdk::FbxDouble eplislon_ = defaultEplislon) {
  for (int i = 0; i < Size_; ++i) {
    if (!isApproximatelyEqual(from_[i], to_[i], eplislon_)) {
      return false;
    }
  }
  return true;
}

fbxsdk::FbxDouble
lerp(fbxsdk::FbxDouble from_, fbxsdk::FbxDouble to_, fbxsdk::FbxDouble rate_) {
  return from_ + (to_ - from_) * rate_;
}

fbxsdk::FbxVector4 lerp(const fbxsdk::FbxVector4 &from_,
                        const fbxsdk::FbxVector4 &to_,
                        fbxsdk::FbxDouble rate_) {
  return fbxsdk::FbxVector4(
      lerp(from_[0], to_[0], rate_), lerp(from_[1], to_[1], rate_),
      lerp(from_[2], to_[2], rate_), lerp(from_[3], to_[3], rate_));
}

fbxsdk::FbxQuaternion slerp(const fbxsdk::FbxQuaternion &from_,
                            const fbxsdk::FbxQuaternion &to_,
                            fbxsdk::FbxDouble rate_) {
  return from_.Slerp(to_, rate_);
}

namespace bee {
/// <summary>
/// Animation on node {} has too long animation. Usually because negative timeline
/// </summary>
class InvalidNodeAnimationRange : public NodeError<InvalidNodeAnimationRange> {
public:
  constexpr static inline std::u8string_view code =
      u8"invalid_node_animation_range";

  const double limit;

  const double received;

  const std::string take_name;

  InvalidNodeAnimationRange(const std::string_view &node_,
                            const double limit_,
                            const double received_,
                            const std::string_view take_name_)
      : NodeError(node_), limit(limit_), received(received_),
        take_name(take_name_) {
  }
};

void to_json(nlohmann::json &j_, const InvalidNodeAnimationRange &error_) {
  to_json(j_,
          static_cast<const NodeError<InvalidNodeAnimationRange> &>(error_));
  j_["take"] = error_.take_name;
  j_["limit"] = error_.limit;
  j_["received"] = error_.received;
}

void SceneConverter::_convertAnimation(fbxsdk::FbxScene &fbx_scene_) {
  if (_animationTimeMode == fbxsdk::FbxTime::eDefaultMode) {
    _log(Logger::Level::error, "The FBX model did not specify a valid time "
                               "mode. You need to specify it manually.");
    return;
  }

  const auto nAnimStacks = fbx_scene_.GetSrcObjectCount<fbxsdk::FbxAnimStack>();
  for (std::remove_const_t<decltype(nAnimStacks)> iAnimStack = 0;
       iAnimStack < nAnimStacks; ++iAnimStack) {
    const auto animStack =
        fbx_scene_.GetSrcObject<fbxsdk::FbxAnimStack>(iAnimStack);
    const auto nAnimLayers = animStack->GetMemberCount<fbxsdk::FbxAnimLayer>();
    if (!nAnimLayers) {
      if (_options.verbose) {
        _log(Logger::Level::verbose,
             u8"There is no animation layer exists in the animation stack.");
      }
      continue;
    }

    const auto timeSpan = _getAnimStackTimeSpan(*animStack, fbx_scene_);
    if (timeSpan.GetDuration() == 0) {
      if (_options.verbose) {
        _log(Logger::Level::verbose, u8"The animation layer's duration is 0.");
      }
      continue;
    }
    const auto usedAnimationTimeMode = _animationTimeMode;
    _log(Logger::Level::verbose,
         fmt::format("Frame rate: {}",
                     fbxsdk::FbxTime::GetFrameRate(_animationTimeMode)));
    const auto firstFrame =
        timeSpan.GetStart().GetFrameCount(usedAnimationTimeMode);
    // It may not be integer multiple, we do ceil thereof.
    const auto lastFrame = static_cast<fbxsdk::FbxLongLong>(std::ceil(
        timeSpan.GetStop().GetFrameCountPrecise(usedAnimationTimeMode)));

    assert(lastFrame >= firstFrame);
    AnimRange animRange{usedAnimationTimeMode, firstFrame, lastFrame};

    fx::gltf::Animation glTFAnimation;
    const auto animName = _convertName(animStack->GetName());
    glTFAnimation.name = animName;

    _log(Logger::Level::verbose,
         fmt::format("Take {}: {}s", animName,
                     timeSpan.GetDuration().GetSecondDouble()));

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

namespace {
constexpr double maxAllowedAnimDurationSeconds = 60.0 * 10.0;
}

fbxsdk::FbxTimeSpan
SceneConverter::_getAnimStackTimeSpan(fbxsdk::FbxAnimStack &fbx_anim_stack_,
                                      fbxsdk::FbxScene &fbx_scene_) {
  const auto nAnimLayers =
      fbx_anim_stack_.GetMemberCount<fbxsdk::FbxAnimLayer>();
  if (!nAnimLayers) {
    return {};
  }

  const auto localTimeSpan = fbx_anim_stack_.GetLocalTimeSpan();
  _log(Logger::Level::verbose,
       "Local time: [" +
           std::to_string(localTimeSpan.GetStart().GetSecondDouble()) + ", " +
           std::to_string(localTimeSpan.GetStop().GetSecondDouble()) + "]");
  const auto referenceTimeSpan = fbx_anim_stack_.GetReferenceTimeSpan();
  _log(Logger::Level::verbose,
       "Reference time: [" +
           std::to_string(referenceTimeSpan.GetStart().GetSecondDouble()) +
           ", " +
           std::to_string(referenceTimeSpan.GetStop().GetSecondDouble()) + "]");

  if (_options.prefer_local_time_span) {
    if (localTimeSpan.GetStart() != 0 || localTimeSpan.GetStop() != 0) {
      return localTimeSpan;
    }
  }

  std::optional<fbxsdk::FbxTimeSpan> animTimeSpan;
  for (std::remove_const_t<decltype(nAnimLayers)> iAnimLayer = 0;
       iAnimLayer < nAnimLayers; ++iAnimLayer) {
    const auto animLayer =
        fbx_anim_stack_.GetMember<fbxsdk::FbxAnimLayer>(iAnimLayer);

    const std::function<void(fbxsdk::FbxNode &)> getInterval =
        [&](fbxsdk::FbxNode &fbx_node_) {
          fbxsdk::FbxTimeSpan interval;
          if (fbx_node_.GetAnimationInterval(interval, &fbx_anim_stack_,
                                             iAnimLayer)) {
            if (const auto duration = interval.GetDuration().GetSecondDouble();
                duration > maxAllowedAnimDurationSeconds) {
              _log(Logger::Level::warning,
                   InvalidNodeAnimationRange{
                       fbx_node_.GetName(), maxAllowedAnimDurationSeconds,
                       duration, fbx_anim_stack_.GetName()});
            } else if (animTimeSpan) {
              animTimeSpan->UnionAssignment(interval);
            } else {
              animTimeSpan = interval;
            }
          }

          const auto nChilds = fbx_node_.GetChildCount();
          for (std::remove_const_t<decltype(nChilds)> iChild = 0;
               iChild < nChilds; ++iChild) {
            auto childNode = fbx_node_.GetChild(iChild);
            getInterval(*childNode);
          }
        };

    getInterval(*fbx_scene_.GetRootNode());
  }

  return animTimeSpan.value_or(fbxsdk::FbxTimeSpan{});
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
    std::vector<std::optional<MorphAnimation>> morphAnimations{
        fbxMeshes.size()};
    for (decltype(fbxMeshes.size()) iMesh = 0; iMesh < fbxMeshes.size();
         ++iMesh) {
      const auto &blendShapeData = blendShapeMeta->blendShapeDatas[iMesh];
      morphAnimations[iMesh] = _extractWeightsAnimation(
          fbx_anim_layer_, fbx_node_, *fbxMeshes[iMesh], blendShapeData,
          anim_range_);
    }

    if (const auto &first = morphAnimations.front();
        // They should all have or have not morph animation
        std::all_of(std::next(morphAnimations.begin()), morphAnimations.end(),
                    [&first](const std::optional<MorphAnimation> &anim_) {
                      return anim_.has_value() == first.has_value();
                    }) &&
        // If they all have morph animation, their morph animation should be
        // compared to equal
        (!first.has_value() ||
         std::all_of(std::next(morphAnimations.begin()), morphAnimations.end(),
                     [&first](const std::optional<MorphAnimation> &anim_) {
                       return anim_->times == first->times &&
                              anim_->values == first->values;
                     }))) {
      if (first) {
        _writeMorphAnimtion(glTF_animation_, *first, nodeBumpMeta.glTFNodeIndex,
                            fbx_node_);
      }
    } else {
      _log(Logger::Level::warning,
           fmt::format("Sub-meshes use different morph animation. We can't "
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

std::optional<SceneConverter::MorphAnimation>
SceneConverter::_extractWeightsAnimation(
    fbxsdk::FbxAnimLayer &fbx_anim_layer_,
    const fbxsdk::FbxNode &fbx_node_,
    fbxsdk::FbxMesh &fbx_mesh_,
    const FbxBlendShapeData &blend_shape_data_,
    const AnimRange &anim_range_) {
  // Zero check
  bool hasWeightAnimation = false;
  for (const auto &[blendShapeIndex, blendShapeChannelIndex, name,
                    deformPercent, targetShapes] : blend_shape_data_.channels) {
    const auto shapeChannel = fbx_mesh_.GetShapeChannel(
        blendShapeIndex, blendShapeChannelIndex, &fbx_anim_layer_);
    if (shapeChannel) {
      hasWeightAnimation = true;
      break;
    }
  }
  if (!hasWeightAnimation) {
    return std::nullopt;
  }

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
  morphAnimation.times.resize(nFrames, 0.0);
  morphAnimation.values.resize(nTargetWeights * nFrames);

  auto extractFrame =
      [](decltype(MorphAnimation::values)::iterator out_weights_,
         fbxsdk::FbxTime time_, fbxsdk::FbxAnimCurve *shape_channel_,
         const decltype(FbxBlendShapeData::Channel::targetShapes)
             &target_shapes_) {
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
                    &target_) { return std::get<1>(target_) >= animWeight; });

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

  const auto firstTimeDouble = anim_range_.first_frame_seconds();
  for (decltype(morphAnimation.times.size()) iFrame = 0;
       iFrame < morphAnimation.times.size(); ++iFrame) {
    const auto time = anim_range_.at(iFrame);

    morphAnimation.times[iFrame] = time.GetSecondDouble() - firstTimeDouble;

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

  const auto nFrames = anim_range_.frames_count();
  std::vector<double> times;
  std::vector<fbxsdk::FbxVector4> translations;
  std::vector<fbxsdk::FbxQuaternion> rotations;
  std::vector<fbxsdk::FbxVector4> scales;

  const auto firstTimeDouble = anim_range_.first_frame_seconds();
  for (std::remove_const_t<decltype(nFrames)> iFrame = 0; iFrame < nFrames;
       ++iFrame) {
    const auto fbxTime = anim_range_.at(iFrame);

    const auto &localTransform = fbx_node_.EvaluateLocalTransform(fbxTime);

    const auto time = fbxTime.GetSecondDouble() - firstTimeDouble;
    fbxsdk::FbxVector4 translation;
    if (isTranslationAnimated) {
      translation = _applyUnitScaleFactorV3(localTransform.GetT());
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

    const auto lastFrameMayBeOptOut = [&]() {
      // We have to admit that we at least get 3 frames
      // even they are linear at all.
      if (times.size() < 2) {
        return false;
      }

      const auto iLast = times.size() - 1;
      const auto iLastLast = times.size() - 2;
      const auto lastTime = times[iLast];
      const auto lastLastTime = times[iLastLast];
      const auto dTime = time - lastTime;
      if (isApproximatelyEqual(dTime, 0.0)) {
        // If current frame time is indistinguishable from previous frame time,
        // we can omit it.
        return true;
      }
      const auto rate = (lastTime - lastLastTime) / dTime;
      if (isApproximatelyEqual(rate, 0.0)) {
        // If two previous frame times are indistinguishable, we shall not omit
        // current frame.
        return false;
      }
      if (isTranslationAnimated &&
          !isApproximatelyEqual<3>(
              lerp(translations[iLastLast], translation, rate),
              translations[iLast])) {
        return false;
      }
      if (isRotationAnimated &&
          !isApproximatelyEqual<4>(slerp(rotations[iLastLast], rotation, rate),
                                   rotations[iLast])) {
        return false;
      }
      if (isScaleAnimated &&
          !isApproximatelyEqual<3>(lerp(scales[iLastLast], scale, rate),
                                   scales[iLast])) {
        return false;
      }
      return true;
    };

    const auto replaceLastFrame = lastFrameMayBeOptOut();
    if (replaceLastFrame) {
      assert(!times.empty());
      times.back() = time;
      if (isTranslationAnimated) {
        translations.back() = translation;
      }
      if (isRotationAnimated) {
        rotations.back() = rotation;
      }
      if (isScaleAnimated) {
        scales.back() = scale;
      }
    } else {
      times.push_back(time);
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