
#pragma once

#include <cstdint>
#include <fx/gltf.h>

namespace bee {
inline constexpr std::uint32_t countComponents(fx::gltf::Accessor::Type type_) {
  switch (type_) {
  default:
  case fx::gltf::Accessor::Type::None:
    assert(false);
    return 0;
  case fx::gltf::Accessor::Type::Scalar:
    return 1;
  case fx::gltf::Accessor::Type::Vec2:
    return 2;
  case fx::gltf::Accessor::Type::Vec3:
    return 3;
  case fx::gltf::Accessor::Type::Vec4:
    return 4;
  case fx::gltf::Accessor::Type::Mat2:
    return 4;
  case fx::gltf::Accessor::Type::Mat3:
    return 9;
  case fx::gltf::Accessor::Type::Mat4:
    return 16;
  }
}

inline constexpr std::uint32_t
countBytes(fx::gltf::Accessor::ComponentType component_type_) {
  switch (component_type_) {
  default:
  case fx::gltf::Accessor::ComponentType::None:
    assert(false);
    return 0;
  case fx::gltf::Accessor::ComponentType::Byte:
  case fx::gltf::Accessor::ComponentType::UnsignedByte:
    return 1;
  case fx::gltf::Accessor::ComponentType::Short:
  case fx::gltf::Accessor::ComponentType::UnsignedShort:
    return 2;
  case fx::gltf::Accessor::ComponentType::UnsignedInt:
  case fx::gltf::Accessor::ComponentType::Float:
    return 4;
  }
}

inline std::uint32_t
countBytes(fx::gltf::Accessor::Type type_,
           fx::gltf::Accessor::ComponentType component_type_) {
  return countBytes(component_type_) * countComponents(type_);
}

template <fx::gltf::Accessor::ComponentType>
struct GetGLTFComponentTypeStorage {};
template <>
struct GetGLTFComponentTypeStorage<fx::gltf::Accessor::ComponentType::Float> {
  using type = float;
};
template <>
struct GetGLTFComponentTypeStorage<
    fx::gltf::Accessor::ComponentType::UnsignedInt> {
  using type = std::uint32_t;
};
template <>
struct GetGLTFComponentTypeStorage<
    fx::gltf::Accessor::ComponentType::UnsignedShort> {
  using type = std::uint16_t;
};

template <fx::gltf::Accessor::ComponentType Component_>
using GLTFComponentTypeStorage =
    typename GetGLTFComponentTypeStorage<Component_>::type;
} // namespace bee