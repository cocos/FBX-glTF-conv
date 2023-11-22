
#include "./String.h"
#define UTF_CPP_CPLUSPLUS 201703L
#include <utf8cpp/utf8.h>
#include <string_view>

namespace bee {
std::string fbx_string_to_utf8_checked(const fbxsdk::FbxString &fbx_string_) {
  std::string_view sw{fbx_string_.Buffer(), fbx_string_.GetLen()};
  const auto replaced = utf8::replace_invalid(sw, u8'?');
  return replaced;
}
} // namespace bee