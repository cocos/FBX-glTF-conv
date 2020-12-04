
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
    std::function<const Value_ &(const FbxLayerElementAccessParams &params_)>;

template <typename Value_>
FbxLayerElementAccessor<Value_> makeFbxLayerElementAccessor(
    const fbxsdk::FbxLayerElementTemplate<Value_> &layer_element_,
    Value_ default_value_ = Value_{}) {

  const auto getVal =
      [&layer_element_]() -> std::function<const Value_ &(int i_)> {
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

  switch (const auto mappingMode = layer_element_.GetMappingMode()) {
    using EMappingMode = fbxsdk::FbxLayerElement::EMappingMode;
  case EMappingMode::eByControlPoint:
    return [getVal](const FbxLayerElementAccessParams &params_) {
      return getVal(params_.controlPointIndex);
    };
  case EMappingMode::eByPolygonVertex:
    return [getVal](const FbxLayerElementAccessParams &params_) {
      return getVal(params_.polygonVertexIndex);
    };
  case EMappingMode::eByPolygon:
    return [getVal](const FbxLayerElementAccessParams &params_) {
      return getVal(params_.polygonIndex);
    };
  case EMappingMode::eByEdge:
    throw std::runtime_error("Unsupported mapping mode: ByEdge");
    break;
  case EMappingMode::eAllSame: {
    auto first = layer_element_.GetDirectArray().GetCount() != 0
                     ? getVal(0)
                     : default_value_;
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
} // namespace bee