
#include <bee/Convert/ConvertError.h>

namespace bee {} // namespace bee

namespace nlohmann {
void adl_serializer<fbxsdk::FbxLayerElement::EMappingMode>::to_json(
    json &j_, fbxsdk::FbxLayerElement::EMappingMode mapping_mode_) {
  switch (mapping_mode_) {
  case fbxsdk::FbxLayerElement::EMappingMode::eNone:
    j_ = "None";
    break;
  case fbxsdk::FbxLayerElement::EMappingMode::eByControlPoint:
    j_ = "ByControlPoint";
    break;
  case fbxsdk::FbxLayerElement::EMappingMode::eByPolygonVertex:
    j_ = "ByPolygonVertex";
    break;
  case fbxsdk::FbxLayerElement::EMappingMode::eByPolygon:
    j_ = "ByPolygon";
    break;
  case fbxsdk::FbxLayerElement::EMappingMode::eByEdge:
    j_ = "ByEdge";
    break;
  case fbxsdk::FbxLayerElement::EMappingMode::eAllSame:
    j_ = "AllSame";
    break;
  default:
    assert(false);
    break;
  }
}
} // namespace nlohmann
