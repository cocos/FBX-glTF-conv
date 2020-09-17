
#pragma once

#ifdef BEE_POLYFILLS_STD_FILESYSTEM
#include <ghc-filesystem/filesystem.hpp>
#include <string_view>
namespace bee::filesystem {
using namespace ghc::filesystem;
} // namespace bee::filesystem
#else
#include <filesystem>
namespace bee::filesystem {
using namespace std::filesystem;
} // namespace bee::filesystem
#endif
