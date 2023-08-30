#pragma once

#include <string>

namespace beecli {
#ifndef FBX_GLTF_CONV_CLI_VERSION
#  define FBX_GLTF_CONV_CLI_VERSION_STRING ""
#else
#  define TO_FBX_GLTF_CONV_CLI_VERSION_STRING(x) #x
#  define TO_FBX_GLTF_CONV_CLI_VERSION_STRING2(x) TO_FBX_GLTF_CONV_CLI_VERSION_STRING(x)
#  define FBX_GLTF_CONV_CLI_VERSION_STRING TO_FBX_GLTF_CONV_CLI_VERSION_STRING2(FBX_GLTF_CONV_CLI_VERSION)
#endif

inline const std::string version_string = FBX_GLTF_CONV_CLI_VERSION_STRING;
} // namespace beecli