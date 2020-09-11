
#pragma once

#include <fx/gltf.h>

namespace bee {
struct GLTFSamplerKeys {
  fx::gltf::Sampler::MagFilter magFilter{fx::gltf::Sampler::MagFilter::None};
  fx::gltf::Sampler::MinFilter minFilter{fx::gltf::Sampler::MinFilter::None};
  fx::gltf::Sampler::WrappingMode wrapS{
      fx::gltf::Sampler::WrappingMode::Repeat};
  fx::gltf::Sampler::WrappingMode wrapT{
      fx::gltf::Sampler::WrappingMode::Repeat};

  bool operator==(const GLTFSamplerKeys &rhs_) const {
    return minFilter == rhs_.minFilter && magFilter == rhs_.magFilter &&
           wrapS == rhs_.wrapS && wrapT == rhs_.wrapT;
  }

  void set(fx::gltf::Sampler &sampler_) {
    sampler_.minFilter = minFilter;
    sampler_.magFilter = magFilter;
    sampler_.wrapS = wrapS;
    sampler_.wrapT = wrapT;
  }
};

struct GLTFSamplerHash {
  std::size_t operator()(const GLTFSamplerKeys &sampler_) const {
    return 0;
  }
};
} // namespace bee