#include "./SplitMeshByMaterial.h"
#include <algorithm>
#include <cassert>
#include <optional>
#include <range/v3/all.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bee {
namespace {
void collect_meshes(fbxsdk::FbxNode &node_, std::unordered_set<fbxsdk::FbxMesh *> &result_) {
  for (const auto iNodeAttribute : ranges::iota_view(0, node_.GetNodeAttributeCount())) {
    const auto nodeAttribute = node_.GetNodeAttributeByIndex(iNodeAttribute);
    switch (const auto attributeType = nodeAttribute->GetAttributeType()) {
    case fbxsdk::FbxNodeAttribute::EType::eMesh:
      result_.emplace(static_cast<fbxsdk::FbxMesh *>(nodeAttribute));
      break;
    default:
      break;
    }
  }
}

void collect_meshes_recurse(fbxsdk::FbxNode &node_, std::unordered_set<fbxsdk::FbxMesh *> &result_) {
  collect_meshes(node_, result_);
  for (const auto iChild : ranges::iota_view(0, node_.GetChildCount())) {
    collect_meshes_recurse(*node_.GetChild(iChild), result_);
  }
}

std::unordered_set<fbxsdk::FbxMesh *> collect_meshes(fbxsdk::FbxScene &scene_) {
  std::unordered_set<fbxsdk::FbxMesh *> result;
  collect_meshes_recurse(*scene_.GetRootNode(), result);
  return result;
}
} // namespace

SplitMeshesResult split_meshes_per_material(fbxsdk::FbxScene &scene_, fbxsdk::FbxGeometryConverter &geometry_converter_) {
  const auto meshes = collect_meshes(scene_);

  SplitMeshesResult result;

  for (const auto mesh : meshes) {
    assert(mesh->GetNodeCount() && "The mesh has not attached to a node");
    if (!mesh->GetNodeCount()) {
      continue;
    }

    const auto firstNode = mesh->GetNode(0);
    assert(firstNode);

    std::unordered_set<fbxsdk::FbxMesh *> meshesOnThisNodeBefore;
    collect_meshes(*firstNode, meshesOnThisNodeBefore);

    const auto success = geometry_converter_.SplitMeshPerMaterial(mesh, false);

    std::unordered_set<fbxsdk::FbxMesh *> meshesOnThisNodeAfter;
    collect_meshes(*firstNode, meshesOnThisNodeAfter);

    {
      std::vector<fbxsdk::FbxMesh *> removed;
      ranges::copy_if(
          meshesOnThisNodeBefore,
          std::back_inserter(removed),
          [&meshesOnThisNodeAfter](auto mesh_) { return meshesOnThisNodeAfter.find(mesh_) == meshesOnThisNodeAfter.end(); });
      assert(removed.empty() && "fbxsdk did something wrong?");
    }

    // The fbxsdk's result might be incorrect!
    // See test "Split an empty mesh".
    //  if (!success) {
    //    assert(meshesOnThisNodeBefore == meshesOnThisNodeAfter && "fbxsdk did something wrong?");
    //    continue;
    //  }

    std::vector<fbxsdk::FbxMesh *> newlyAdded;
    ranges::copy_if(
        meshesOnThisNodeAfter,
        std::back_inserter(newlyAdded),
        [&meshesOnThisNodeBefore](auto mesh_) { return meshesOnThisNodeBefore.find(mesh_) == meshesOnThisNodeBefore.end(); });
    if (newlyAdded.empty()) {
      assert(meshesOnThisNodeBefore == meshesOnThisNodeAfter && "fbxsdk did something wrong?");
      continue;
    }

    for (const auto split : newlyAdded) {
      split->SetName(mesh->GetName());
      for (const auto node :
           ranges::views::iota(0, split->GetNodeCount()) |
               ranges::views::reverse |
               ranges::views::transform([split](auto i_) { return split->GetNode(i_); })) {
        node->RemoveNodeAttribute(split);
      }
      result.emplace(mesh, split);
    }
  }

  return result;
}
} // namespace bee
