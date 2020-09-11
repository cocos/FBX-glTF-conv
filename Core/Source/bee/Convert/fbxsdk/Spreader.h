
#pragma once

#include <fbxsdk.h>
#include <type_traits>

namespace bee {
template <typename Ty_, typename SizeType_, SizeType_ Size_>
struct ArraySpreader {
  using type = Ty_;

  constexpr static SizeType_ size = Size_;

  template <typename TargetTy_>
  static void spread(const type &in_, TargetTy_ *out_) {
    for (std::remove_const_t<decltype(size)> i = 0; i < size; ++i) {
      out_[i] = static_cast<TargetTy_>(in_[i]);
    }
  }
};

struct FbxVec2Spreader : ArraySpreader<fbxsdk::FbxVector2, int, 2> {};

struct FbxVec3Spreader : ArraySpreader<fbxsdk::FbxVector4, int, 3> {};

struct FbxQuatSpreader : ArraySpreader<fbxsdk::FbxQuaternion, int, 4> {};

struct FbxColorSpreader : ArraySpreader<fbxsdk::FbxColor, int, 4> {};

struct FbxAMatrixSpreader {
  using type = fbxsdk::FbxAMatrix;

  constexpr static int size = 16;

  template <typename TargetTy_>
  static void spread(const type &in_, TargetTy_ *out_) {
    for (std::remove_const_t<decltype(size)> i = 0; i < size; ++i) {
      out_[i] = static_cast<TargetTy_>(
          static_cast<const fbxsdk::FbxDouble *>(in_)[i]);
    }
  }
};
} // namespace bee