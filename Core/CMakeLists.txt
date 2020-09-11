
set (BeeCoreIncludeDirectories "${CMAKE_CURRENT_LIST_DIR}/Source")

set (BeeCoreSource
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/BEE_API.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Converter.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Converter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/GLTFBuilder.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/GLTFBuilder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/UntypedVertex.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/UntypedVertex.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/GLTFUtilities.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/GLTFUtilities.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/ObjectDestroyer.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/LayerelementAccessor.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/Spreader.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.Mesh.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.BlendShape.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.Skin.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.Animation.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.Material.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/SceneConverter.Texture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/GLTFSamplerHash.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/FbxMeshVertexLayout.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/DirectSpreader.h"
    )

add_library (BeeCore SHARED ${BeeCoreSource})

set_target_properties (BeeCore PROPERTIES CXX_STANDARD 20)

target_compile_definitions (BeeCore PRIVATE BEE_EXPORT FBXSDK_SHARED NOMINMAX)

target_include_directories (BeeCore PRIVATE ${BeeCoreIncludeDirectories} "${CMAKE_CURRENT_LIST_DIR}/fx/include")

message (STATUS "FBXSDK: ${FbxSdkHome}")

set (FbxSdkIncludeDirectories "${FbxSdkHome}/include")
set (FbxSdkLibraries "${FbxSdkHome}/lib/vs2017/x64/release/libfbxsdk.lib")
set (FbxSdkDynLibraries "${FbxSdkHome}/lib/vs2017/x64/release/libfbxsdk.dll")

target_include_directories (BeeCore PRIVATE ${FbxSdkIncludeDirectories})

target_link_libraries (BeeCore PRIVATE ${FbxSdkLibraries})

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(BeeCore PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(BeeCore PRIVATE fmt::fmt fmt::fmt-header-only)

find_package(skyr-url CONFIG REQUIRED)
## https://github.com/cpp-netlib/url/issues/143
set_property(TARGET skyr::skyr-url APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(skyr::skyr-url PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_VCPKG_ROOT_DIR}/installed/x64-windows/debug/lib/skyr-urld.lib"
)
target_link_libraries(BeeCore PRIVATE skyr::skyr-url skyr::skyr-json skyr::skyr-filesystem)