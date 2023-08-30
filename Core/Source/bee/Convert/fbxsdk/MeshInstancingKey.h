
#pragma once

#include <fbxsdk.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bee {
class MeshInstancingKey {
public:
  template <typename>
  friend struct std::hash;

  MeshInstancingKey(
      std::unordered_set<fbxsdk::FbxMesh *> &&meshes_,
      fbxsdk::FbxNode &node_);

  bool operator==(const MeshInstancingKey &) const = default;
  bool operator!=(const MeshInstancingKey &) const = default;

private:
  std::unordered_set<fbxsdk::FbxMesh *> _meshes;
  std::vector<fbxsdk::FbxSurfaceMaterial *> _nodeMaterials;
};
} // namespace bee

template <>
struct std::hash<bee::MeshInstancingKey> {
  std::size_t operator()(const bee::MeshInstancingKey &key_) const noexcept {
    return key_._meshes.size();
  }
};