
#include <algorithm>
#include <bee/Convert/fbxsdk/MeshInstancingKey.h>
#include <range/v3/all.hpp>

namespace bee {
MeshInstancingKey::MeshInstancingKey(
    std::unordered_set<fbxsdk::FbxMesh *> &&meshes_,
    fbxsdk::FbxNode &node_)
    : _meshes(std::move(meshes_)) {
  ranges::copy(ranges::views::iota(0, node_.GetMaterialCount()) |
                   ranges::views::transform([&node_](auto index_) { return node_.GetMaterial(index_); }),
               std::back_inserter(_nodeMaterials));
}
} // namespace bee