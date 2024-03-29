
#include <algorithm>
#include <bee/Convert/fbxsdk/MeshInstancingKey.h>
#include <range/v3/all.hpp>

namespace bee {
MeshInstancingKey::MeshInstancingKey(
    std::unordered_set<fbxsdk::FbxMesh *> &&meshes_,
    fbxsdk::FbxNode &node_)
    : _meshes(std::move(meshes_)) {
  ranges::copy(ranges::iota_view<int, int>(0, node_.GetMaterialCount()) |
                   ranges::views::transform([&node_](auto index_) { return node_.GetMaterial(index_); }),
               ranges::back_inserter(_nodeMaterials));
}
} // namespace bee