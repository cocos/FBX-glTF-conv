#pragma once

#include <fbxsdk/core/base/fbxstring.h>
#include <string>

namespace bee {
std::string fbx_string_to_utf8_checked(const fbxsdk::FbxString &fbx_string_);
} // namespace bee