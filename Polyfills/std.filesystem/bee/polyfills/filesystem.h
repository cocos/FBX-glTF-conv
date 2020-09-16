
#pragma once

#ifdef BEE_POLYFILLS_STD_FILESYSTEM
#include <ghc-filesystem/filesystem.hpp>
namespace bee::filesystem {
using namespace ghc::filesystem;
}
#else
#include <filesystem>
namespace bee::filesystem {
using namespace std::filesystem;
}
#endif
