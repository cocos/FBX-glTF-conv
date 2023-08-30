
#pragma once

#include <fbxsdk.h>
#include <unordered_map>

namespace bee {
using SplitMeshesResult = std::unordered_multimap<fbxsdk::FbxMesh *, fbxsdk::FbxMesh *>;

SplitMeshesResult split_meshes_per_material(fbxsdk::FbxScene &scene_, fbxsdk::FbxGeometryConverter &geometry_converter_);
} // namespace bee