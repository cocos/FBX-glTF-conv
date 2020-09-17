
#pragma once

#ifdef BEE_POLYFILLS_STD_FILESYSTEM

// ghc::filesystem in their source uses
// the C++ 20 macro `__cpp_lib_string_view`(defined in header `<version>`) to
// determinate whether to include the string view related functionalities. As
// far as I can see, `__cpp_lib_string_view` is weird on macOS(Apple clang):
// - Compiler flag `-std=c++2a` and header `<string_view>` was available since
// macOS 10.13;
// - but even in the latest macOS(10.14), the `__cpp_lib_string_view` is not
// avaiable:
//   - For 10.13, no `<version>`;
//   - for 10.14, the `<version>` is presented but not used.
//     Instead, `<__cxx_version>` is used at anywhere(such as implicitly
//     included in `<string_view>`) but `<__cxx_version>` has no
//     `__cpp_lib_string_view` defined.
// To solve this issue, we do some HACKs here.
#include <string_view> // As stated by specification, `<string_view>` implies `__cpp_lib_string_view`.
#ifndef __cpp_lib_string_view
#if __cplusplus >= 202002L // >= C++ 20
#define __cpp_lib_string_view 201803L
#define BEE_POLYFILLS_STD_FILESYSTEM_DEFINE_CPP_LIB_STRING_VIEW
#elif __cplusplus >= 201703L // >= C++ 17
#define __cpp_lib_string_view 201606L
#define BEE_POLYFILLS_STD_FILESYSTEM_DEFINE_CPP_LIB_STRING_VIEW
#endif
#endif

#include <ghc-filesystem/filesystem.hpp>

// Restore..
#ifdef BEE_POLYFILLS_STD_FILESYSTEM_DEFINE_CPP_LIB_STRING_VIEW
#undef BEE_POLYFILLS_STD_FILESYSTEM_DEFINE_CPP_LIB_STRING_VIEW
#undef __cpp_lib_string_view
#endif

namespace bee::filesystem {
using namespace ghc::filesystem;
} // namespace bee::filesystem

#else

#include <filesystem>
namespace bee::filesystem {
using namespace std::filesystem;
} // namespace bee::filesystem

#endif
