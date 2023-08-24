#pragma once

#include <bee/GLTFBuilder.h>
#include <string_view>

namespace bee::detail::glTF_extension::khr_animation_pointer {
inline const static std::string_view extension_name = "KHR_animation_pointer";

inline fx::gltf::Animation::Channel::Target
make_target(GLTFBuilder &builder_, std::string_view pointer_) {
  fx::gltf::Animation::Channel::Target target;

  target.path = "pointer";
  target.extensionsAndExtras["extensions"]
                            [detail::glTF_extension::khr_animation_pointer::
                                 extension_name]["pointer"] = pointer_;

  builder_.requireExtension(
      detail::glTF_extension::khr_animation_pointer::extension_name);

  return target;
}
} // namespace bee::detail::glTF_extension::khr_animation_pointer