
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
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/ConvertError.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/ConvertError.cpp"
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
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/String.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/String.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/SplitMeshByMaterial.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/SplitMeshByMaterial.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/MeshInstancingKey.h"
    "${CMAKE_CURRENT_LIST_DIR}/Source/bee/Convert/fbxsdk/MeshInstancingKey.cpp"
    )

add_library (BeeCore SHARED ${BeeCoreSource})

set_target_properties (BeeCore PROPERTIES CXX_STANDARD 20)

target_compile_definitions (BeeCore PRIVATE BEE_EXPORT FBXSDK_SHARED NOMINMAX HAVE_SNPRINTF)

target_include_directories (BeeCore PRIVATE ${BeeCoreIncludeDirectories} "${CMAKE_CURRENT_LIST_DIR}/fx/include")

target_include_directories (BeeCore PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../Polyfills/nlohmann-json")

target_include_directories (BeeCore PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../Polyfills/std.filesystem")
if (POLYFILLS_STD_FILESYSTEM)
    message (STATUS "We're using polyfill <filesystem>")
    target_compile_definitions (BeeCore PRIVATE BEE_POLYFILLS_STD_FILESYSTEM)
endif ()

if (NOT DEFINED FbxSdkHome)
    message(FATAL_ERROR "You must specify the FBX SDK home.")
endif ()
message (STATUS "FBX SDK home: ${FbxSdkHome}")

# https://blog.kitware.com/cmake-and-the-default-build-type/
message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  set (FbxSdkConfiguration "debug")
else ()
  set (FbxSdkConfiguration "release")
endif ()
message (STATUS "FBX SDK configuration: ${FbxSdkConfiguration}")

message (STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
set (FbxSdkIncludeDirectories "${FbxSdkHome}/include")
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if (EXISTS "${FbxSdkHome}/lib/vs2022")
        set (FbxSdkLibraries "${FbxSdkHome}/lib/vs2022/x64/${FbxSdkConfiguration}/libfbxsdk.lib")
        set (FbxSdkDynLibraries "${FbxSdkHome}/lib/vs2022/x64/${FbxSdkConfiguration}/libfbxsdk.dll")
    elseif (EXISTS "${FbxSdkHome}/lib/vs2019")
        set (FbxSdkLibraries "${FbxSdkHome}/lib/vs2019/x64/${FbxSdkConfiguration}/libfbxsdk.lib")
        set (FbxSdkDynLibraries "${FbxSdkHome}/lib/vs2019/x64/${FbxSdkConfiguration}/libfbxsdk.dll")
    else ()
        set (FbxSdkLibraries "${FbxSdkHome}/lib/vs2017/x64/${FbxSdkConfiguration}/libfbxsdk.lib")
        set (FbxSdkDynLibraries "${FbxSdkHome}/lib/vs2017/x64/${FbxSdkConfiguration}/libfbxsdk.dll")
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set (FbxSdkLibraries "${FbxSdkHome}/lib/clang/${FbxSdkConfiguration}/libfbxsdk.a")
    set (FbxSdkDynLibraries "${FbxSdkHome}/lib/clang/${FbxSdkConfiguration}/libfbxsdk.dylib")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (FbxSdkLibraries "${FbxSdkHome}/lib/gcc/x64/${FbxSdkConfiguration}/libfbxsdk.a")
    set (FbxSdkDynLibraries "${FbxSdkHome}/lib/gcc/x64/${FbxSdkConfiguration}/libfbxsdk.so")
else ()
    message (FATAL_ERROR "Can not decide FBX SDK library to use cause of unknown C++ compiler ID: ${CMAKE_CXX_COMPILER_ID}")
endif ()
message (STATUS "FBX SDK include directories: ${FbxSdkIncludeDirectories}")
message (STATUS "FBX SDK libraries: ${FbxSdkLibraries}")
message (STATUS "FBX SDK dynamic libraries: ${FbxSdkDynLibraries}")

# `LibXml2`, `ZLIB` are required by FBX SDK
# https://forums.autodesk.com/t5/fbx-forum/cmake-error-quot-undefined-symbols-for-architecture-x86-64-quot/td-p/8992522
find_package(LibXml2 REQUIRED)
target_include_directories(BeeCore PRIVATE ${LIBXML2_INCLUDE_DIR})
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # I don't know why on Ubuntu "PUBLIC" is needed, otherwise there're link errors.
    target_link_libraries(BeeCore PUBLIC LibXml2::LibXml2)
else ()
    target_link_libraries(BeeCore PRIVATE ${LIBXML2_LIBRARIES})
endif ()
find_package(ZLIB REQUIRED)
target_link_libraries(BeeCore PRIVATE ZLIB::ZLIB)

if (APPLE)
    find_library (CF_FRAMEWORK CoreFoundation)
    message("CoreFoundation Framework: ${CF_FRAMEWORK}")
    target_link_libraries(BeeCore PRIVATE ${CF_FRAMEWORK})
endif()

target_include_directories (BeeCore PRIVATE ${FbxSdkIncludeDirectories})

target_link_libraries (BeeCore PRIVATE ${FbxSdkLibraries})

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(BeeCore PRIVATE nlohmann_json nlohmann_json::nlohmann_json)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(BeeCore PRIVATE fmt::fmt fmt::fmt-header-only)

find_path(CPPCODEC_INCLUDE_DIRS "cppcodec/base32_crockford.hpp")
target_include_directories(BeeCore PRIVATE ${CPPCODEC_INCLUDE_DIRS})

#find_package(skyr-url CONFIG REQUIRED)
## https://github.com/cpp-netlib/url/issues/143
#set_property(TARGET skyr::skyr-url APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
#set_target_properties(skyr::skyr-url PROPERTIES
#  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
#  IMPORTED_LOCATION_DEBUG "${_VCPKG_ROOT_DIR}/installed/x64-windows/debug/lib/skyr-urld.lib"
#)
#target_link_libraries(BeeCore PRIVATE skyr::skyr-url skyr::skyr-json skyr::skyr-filesystem)

find_package(range-v3 CONFIG REQUIRED)
target_link_libraries(BeeCore PRIVATE range-v3)

#find_package(utf8cpp CONFIG REQUIRED)
#target_link_libraries(BeeCore PRIVATE utf8cpp)

install (TARGETS BeeCore DESTINATION "bin")
install (FILES $<TARGET_LINKER_FILE:BeeCore> DESTINATION "lib" OPTIONAL)
if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    install (FILES $<TARGET_PDB_FILE:BeeCore> DESTINATION "bin" OPTIONAL)
endif()
install (FILES ${FbxSdkDynLibraries} DESTINATION "bin")

# Testing
if (NOT APPLE) # FIXME: iconv is not found on GitHub macOS release flow
    file (GLOB_RECURSE BeeCoreTestSource
        "${CMAKE_CURRENT_LIST_DIR}/Test/*.cpp"
    )
    find_package(doctest REQUIRED)
    add_executable(FBX-glTF-conv-core-test ${BeeCoreTestSource}) 
    set_target_properties (FBX-glTF-conv-core-test PROPERTIES CXX_STANDARD 20) 
    target_compile_definitions (FBX-glTF-conv-core-test PRIVATE FBXSDK_SHARED)
    target_include_directories(FBX-glTF-conv-core-test PRIVATE ${DOCTEST_INCLUDE_DIR}) 
    target_include_directories (FBX-glTF-conv-core-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Source") 
    target_link_libraries(FBX-glTF-conv-core-test PRIVATE doctest::doctest) 
    target_include_directories (FBX-glTF-conv-core-test PRIVATE ${FbxSdkIncludeDirectories}) 
    target_link_libraries (FBX-glTF-conv-core-test PRIVATE ${FbxSdkLibraries}) 
    target_include_directories (FBX-glTF-conv-core-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../Polyfills/nlohmann-json")
    target_include_directories (FBX-glTF-conv-core-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../Core/fx/include")
    target_link_libraries (FBX-glTF-conv-core-test PUBLIC BeeCore)
    target_link_libraries(FBX-glTF-conv-core-test PRIVATE fmt::fmt fmt::fmt-header-only)
    if (APPLE)
        find_library (CF_FRAMEWORK CoreFoundation)
        message("CoreFoundation Framework: ${CF_FRAMEWORK}")
        target_link_libraries(FBX-glTF-conv-core-test PRIVATE ${CF_FRAMEWORK})
    endif()
endif()