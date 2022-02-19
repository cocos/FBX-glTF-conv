
#pragma once

#include <fbxsdk.h>
#include <functional>

namespace bee {
struct FbxLayerElementAccessParams {
  int controlPointIndex = 0;
  int polygonVertexIndex = 0;
  int polygonIndex = 0;
};

template <typename Value_>
using FbxLayerElementAccessor =
    std::function<Value_(const FbxLayerElementAccessParams &params_)>;

template <typename Value>
std::function<Value(const FbxLayerElementAccessParams &params_)>
create_mapping_mode_visitor(
    fbxsdk::FbxLayerElement::EMappingMode mapping_mode_,
    std::function<Value(int index_)> reference_mode_visitor_,
    Value default_value_ = {}) {
  switch (mapping_mode_) {
    using EMappingMode = fbxsdk::FbxLayerElement::EMappingMode;
  case EMappingMode::eByControlPoint:
    return
        [reference_mode_visitor_](const FbxLayerElementAccessParams &params_) {
          return reference_mode_visitor_(params_.controlPointIndex);
        };
  case EMappingMode::eByPolygonVertex:
    return
        [reference_mode_visitor_](const FbxLayerElementAccessParams &params_) {
          return reference_mode_visitor_(params_.polygonVertexIndex);
        };
  case EMappingMode::eByPolygon:
    return
        [reference_mode_visitor_](const FbxLayerElementAccessParams &params_) {
          return reference_mode_visitor_(params_.polygonIndex);
        };
  case EMappingMode::eByEdge:
    throw std::runtime_error("Unsupported mapping mode: ByEdge");
    break;
  case EMappingMode::eAllSame: {
    auto first = reference_mode_visitor_(0);
    return
        [first](const FbxLayerElementAccessParams &params_) { return first; };
  }
  case EMappingMode::eNone:
    return [default_value_](const FbxLayerElementAccessParams &params_) {
      return default_value_;
    };
  default:
    throw std::runtime_error("Unknown mapping mode");
  }
}

template <typename Value_,
          typename = std::enable_if_t<
              !std::is_same_v<Value_, fbxsdk::FbxSurfaceMaterial *>>>
FbxLayerElementAccessor<Value_> makeFbxLayerElementAccessor(
    const fbxsdk::FbxLayerElementTemplate<Value_> &layer_element_,
    Value_ default_value_ = Value_{}) {

  const auto getVal = [&layer_element_]() -> std::function<Value_(int i_)> {
    using EReferenceMode = fbxsdk::FbxLayerElement::EReferenceMode;
    const auto referenceMode = layer_element_.GetReferenceMode();
    if (referenceMode == EReferenceMode::eDirect) {
      auto &directArray = layer_element_.GetDirectArray();
      return [&directArray](int index_) { return directArray[index_]; };
    } else if (referenceMode == EReferenceMode::eIndexToDirect ||
               referenceMode == EReferenceMode::eIndex) {
      auto &directArray = layer_element_.GetDirectArray();
      auto &indexArray = layer_element_.GetIndexArray();
      return [&directArray, &indexArray](int index_) {
        return directArray[indexArray[index_]];
      };
    } else {
      throw std::runtime_error("Unknown reference mode");
    }
  }();

  return create_mapping_mode_visitor(layer_element_.GetMappingMode(), getVal,
                                     default_value_);
}

/// <summary>
/// Material layer element should be handled specially. See `FbxLayerElementMaterial`.
/// </summary>
/// <returns></returns>
inline FbxLayerElementAccessor<int> makeLayerElementMaterialAccessor(
    const fbxsdk::FbxLayerElementMaterial &layer_element_) {
  using Value = int;

  constexpr Value default_value_ = -1;

  const auto getVal = [&layer_element_]() -> std::function<Value(int i_)> {
    using EReferenceMode = fbxsdk::FbxLayerElement::EReferenceMode;
    auto &indexArray = layer_element_.GetIndexArray();

    // >> this type of Layer element should have its reference mode set to
    // >> `eIndexToDirect`.
    // If we encountered such violation, we return a null material.
    using EReferenceMode = fbxsdk::FbxLayerElement::EReferenceMode;
    const auto referenceMode = layer_element_.GetReferenceMode();
    if (!(referenceMode == EReferenceMode::eIndexToDirect ||
          referenceMode == EReferenceMode::eIndex)) {
      return [&indexArray](int index_) { return -1; };
    }

    return [&indexArray](int index_) { return indexArray[index_]; };
  }();

  return create_mapping_mode_visitor(layer_element_.GetMappingMode(), getVal,
                                     default_value_);
}
} // namespace bee