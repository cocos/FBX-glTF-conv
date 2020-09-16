cmake_minimum_required (VERSION 3.8)

project ("FBX-glTF-conv")

option(POLYFILLS_STD_FILESYSTEM "Use Polyfill <filesystem>" OFF)

include ("./Cli/CMakeLists.txt")
