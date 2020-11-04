cmake_minimum_required (VERSION 3.8)

project ("FBX-glTF-conv")

option (POLYFILLS_STD_FILESYSTEM "Use Polyfill <filesystem>" OFF)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

# set (POLYFILLS_STD_FILESYSTEM ON)

include ("./Cli/CMakeLists.txt")
