
#pragma once

#include <fbxsdk.h>
#include <functional>

namespace bee {
template <typename Value_>
using FbxLayerElementAccessor = std::function<const Value_ &(
    int control_point_index_, int polygon_vertex_index_)>;

template <typename Value_>
FbxLayerElementAccessor<Value_> makeFbxLayerElementAccessor(
    const fbxsdk::FbxLayerElementTemplate<Value_> &layer_element_) {
  const auto mappingMode = layer_element_.GetMappingMode();
  const auto referenceMode = layer_element_.GetReferenceMode();
  if (referenceMode == fbxsdk::FbxLayerElement::EReferenceMode::eDirect) {
    auto &directArray = layer_element_.GetDirectArray();
    switch (mappingMode) {
    case fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint:
      return
          [&directArray](int control_point_index_, int polygon_vertex_index_) {
            return directArray[control_point_index_];
          };
    case fbxsdk::FbxLayerElement::EMappingMode::eByPolygonVertex:
      return
          [&directArray](int control_point_index_, int polygon_vertex_index_) {
            return directArray[polygon_vertex_index_];
          };
    default:
      throw std::runtime_error("Unknown mapping mode");
    }
  } else if (referenceMode ==
             fbxsdk::FbxLayerElement::EReferenceMode::eIndexToDirect) {
    auto &directArray = layer_element_.GetDirectArray();
    auto &indexArray = layer_element_.GetIndexArray();
    switch (mappingMode) {
    case fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint:
      return [&directArray, &indexArray](int control_point_index_,
                                         int polygon_vertex_index_) {
        return directArray[indexArray[control_point_index_]];
      };
    case fbxsdk::FbxLayerElement::EMappingMode::eByPolygonVertex:
      return [&directArray, &indexArray](int control_point_index_,
                                         int polygon_vertex_index_) {
        return directArray[indexArray[polygon_vertex_index_]];
      };
    default:
      throw std::runtime_error("Unknown mapping mode");
    }
  } else {
    throw std::runtime_error("Unknown reference mode");
  }
}
} // namespace bee