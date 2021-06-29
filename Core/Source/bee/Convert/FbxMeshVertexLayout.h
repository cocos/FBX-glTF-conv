
#pragma once

#include <bee/Convert/NeutralType.h>
#include <bee/Convert/fbxsdk/LayerelementAccessor.h>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bee {
template <typename Element_> struct FbxMeshAttributeLayout {
  std::uint32_t offset = 0;
  Element_ element;

  FbxMeshAttributeLayout() = default;

  FbxMeshAttributeLayout(std::uint32_t offset_, Element_ element_)
      : offset(offset_), element(element_) {
  }
};

struct FbxMeshVertexLayout {
  std::uint32_t size = sizeof(NeutralVertexComponent) * 3;

  std::optional<FbxMeshAttributeLayout<
      FbxLayerElementAccessor<fbxsdk::FbxLayerElementNormal::ArrayElementType>>>
      normal;

  std::vector<FbxMeshAttributeLayout<
      FbxLayerElementAccessor<fbxsdk::FbxLayerElementUV::ArrayElementType>>>
      uvs;

  std::vector<FbxMeshAttributeLayout<FbxLayerElementAccessor<
      fbxsdk::FbxLayerElementVertexColor::ArrayElementType>>>
      colors;

  struct Skinning {
    std::uint32_t channelCount;
    /// <summary>
    /// Layout offset.
    /// </summary>
    std::uint32_t joints;
    /// <summary>
    /// Layout offset.
    /// </summary>
    std::uint32_t weights;
  };

  std::optional<Skinning> skinning;

  struct ShapeLayout {
    FbxMeshAttributeLayout<fbxsdk::FbxVector4 *> constrolPoints;

    std::optional<FbxMeshAttributeLayout<FbxLayerElementAccessor<
        fbxsdk::FbxLayerElementNormal::ArrayElementType>>>
        normal;
  };

  std::vector<ShapeLayout> shapes;

  std::unordered_map<std::string, std::uint32_t> uv_channel_index_map;
};
} // namespace bee