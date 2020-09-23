
#pragma once

#include <cstddef>

namespace bee {
template <typename Ty_> struct DirectSpreader {
  using type = Ty_;

  constexpr static std::size_t size = 1;

  template <typename TargetTy_>
  static void spread(const type &in_, TargetTy_ *out_) {
    *out_ = static_cast<TargetTy_>(in_);
  }
};
} // namespace bee