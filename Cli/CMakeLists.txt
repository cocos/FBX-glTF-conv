
include ("${CMAKE_CURRENT_LIST_DIR}/../Core/CMakeLists.txt")

add_executable (FBXInglTFCli "${CMAKE_CURRENT_LIST_DIR}/Cli.cpp")

set_target_properties (FBXInglTFCli PROPERTIES CXX_STANDARD 17)

target_include_directories (FBXInglTFCli PRIVATE ${BeeCoreIncludeDirectories})

target_link_libraries (FBXInglTFCli PRIVATE BeeCore)

find_package(clipp CONFIG REQUIRED)

target_link_libraries(FBXInglTFCli PRIVATE clipp::clipp)

add_custom_command(TARGET FBXInglTFCli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FbxSdkHome}/lib/vs2017/x64/release/libfbxsdk.dll"
        $<TARGET_FILE_DIR:FBXInglTFCli>)