
#[[
    We introduced the "FBX-glTF-conv-lib" to do better testing.
    See https://stackoverflow.com/questions/41811674/cmake-add-test-executable
]]

include ("${CMAKE_CURRENT_LIST_DIR}/../Core/CMakeLists.txt")

# LIB

add_library (FBX-glTF-conv-lib STATIC "${CMAKE_CURRENT_LIST_DIR}/ReadCliArgs.cpp")

set_target_properties (FBX-glTF-conv-lib PROPERTIES CXX_STANDARD 20)

target_include_directories (FBX-glTF-conv-lib PUBLIC ${BeeCoreIncludeDirectories})

target_include_directories (FBX-glTF-conv-lib PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../Polyfills/nlohmann-json")

target_include_directories (FBX-glTF-conv-lib PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../Polyfills/std.filesystem")
if (POLYFILLS_STD_FILESYSTEM)
    message (STATUS "We're using polyfill <filesystem>")
    target_compile_definitions (FBX-glTF-conv-lib PUBLIC BEE_POLYFILLS_STD_FILESYSTEM)
endif ()

target_link_libraries (FBX-glTF-conv-lib PUBLIC BeeCore)

find_package(cxxopts CONFIG REQUIRED)
target_link_libraries(FBX-glTF-conv-lib PUBLIC cxxopts::cxxopts)

#[[
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(BeeCore PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(BeeCore PRIVATE fmt::fmt fmt::fmt-header-only)
]]

# -----------------
# Executable

add_executable (FBX-glTF-conv "${CMAKE_CURRENT_LIST_DIR}/Cli.cpp")

set_target_properties (FBX-glTF-conv PROPERTIES CXX_STANDARD 20)

target_link_libraries(FBX-glTF-conv PRIVATE FBX-glTF-conv-lib)

# Set @rpath so executable can find .dylib/.so
# See https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/RPATH-handling#recommendations
# and also https://stackoverflow.com/a/61020959
# and also # https://dev.my-gate.net/2021/08/04/understanding-rpath-with-cmake/
if (APPLE)
    set_target_properties (FBX-glTF-conv PROPERTIES INSTALL_RPATH "@executable_path")
elseif (UNIX)
    set_target_properties (FBX-glTF-conv PROPERTIES INSTALL_RPATH "$ORIGIN")
endif ()

add_custom_command(TARGET FBX-glTF-conv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FbxSdkDynLibraries}
        $<TARGET_FILE_DIR:FBX-glTF-conv>)

install (TARGETS FBX-glTF-conv DESTINATION "bin")

# ------------------
# Testing
find_package(doctest REQUIRED)
add_executable(FBX-glTF-conv-test "${CMAKE_CURRENT_LIST_DIR}/Test/ReadCliArgs.cpp")
set_target_properties (FBX-glTF-conv-test PROPERTIES CXX_STANDARD 20)
target_include_directories(FBX-glTF-conv-test PRIVATE ${DOCTEST_INCLUDE_DIR})
target_include_directories (FBX-glTF-conv-test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(FBX-glTF-conv-test PRIVATE doctest::doctest)
target_link_libraries (FBX-glTF-conv-test PRIVATE FBX-glTF-conv-lib)
target_link_libraries(FBX-glTF-conv-test PRIVATE fmt::fmt fmt::fmt-header-only)
