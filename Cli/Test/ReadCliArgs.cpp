
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "ReadCliArgs.h"
#include <doctest/doctest.h>
#include <string>

using namespace std::literals;

const auto dummyArg0 = "UnitTest";
const auto dummyInput = "/Input.fbx"sv;

std::string u8toexe(std::u8string_view u8sv_) {
  return std::string{u8sv_.begin(), u8sv_.end()};
}

auto read_cli_args_with_dummy_and(std::string_view extra_) {
  std::vector<std::string_view> args{dummyArg0, dummyInput};
  args.insert(args.end(), extra_);
  return beecli::readCliArgs(args);
}

auto read_cli_args_with_dummy_and(std::span<std::string_view> extras_) {
  std::vector<std::string_view> args{dummyArg0, dummyInput};
  args.insert(args.end(), extras_.begin(), extras_.end());
  return beecli::readCliArgs(args);
}

TEST_CASE("Read CLI arguments") {

  { // Empty args
    std::vector<std::string_view> args{dummyArg0};
    beecli::readCliArgs(args);
  }

  { // Help
    std::vector<std::string_view> args{dummyArg0, "--help"};
    beecli::readCliArgs(args);
  }

  { // Default args
    std::vector<std::string_view> args{dummyArg0, dummyInput};
    const auto convertOptions = beecli::readCliArgs(args);
    CHECK_EQ(u8toexe(convertOptions->inputFile), dummyInput);
    CHECK_EQ(u8toexe(convertOptions->outFile), ""s);
    CHECK_EQ(u8toexe(convertOptions->fbmDir), "");
    CHECK_EQ(convertOptions->logFile, std::nullopt);
    CHECK_EQ(convertOptions->convertOptions.prefer_local_time_span, true);
    CHECK_EQ(convertOptions->convertOptions.match_mesh_names, true);
    CHECK_EQ(convertOptions->convertOptions.animationBakeRate, 0);
    CHECK_EQ(convertOptions->convertOptions.animation_position_error_multiplier,
             doctest::Approx(1e-5));
    CHECK_EQ(convertOptions->convertOptions.animation_scale_error_multiplier,
             doctest::Approx(1e-5));
    CHECK_EQ(convertOptions->convertOptions.verbose, false);
    CHECK_EQ(convertOptions->convertOptions.noFlipV, false);
    CHECK_EQ(convertOptions->convertOptions.textureResolution.disabled, false);
    CHECK_EQ(convertOptions->convertOptions.unitConversion,
             bee::ConvertOptions::UnitConversion::geometryLevel);
    CHECK_EQ(convertOptions->convertOptions.export_fbx_file_header_info, false);
    CHECK_EQ(convertOptions->convertOptions.export_raw_materials, false);
  }

  {// Input file
   {std::vector<std::string_view> args{dummyArg0, "--prefer-local-time-span"sv,
                                       dummyInput};
  CHECK_EQ(u8toexe(beecli::readCliArgs(args)->inputFile), dummyInput);
}

{
  std::vector<std::string_view> args{dummyArg0, dummyInput,
                                     "--prefer-local-time-span"sv};
  CHECK_EQ(u8toexe(beecli::readCliArgs(args)->inputFile), dummyInput);
}

{
  std::vector<std::string_view> args{dummyArg0, "--prefer-local-time-span"sv,
                                     "--"sv, dummyInput};
  CHECK_EQ(u8toexe(beecli::readCliArgs(args)->inputFile), dummyInput);
}

{ // CJK, spaces
  const auto cjkFile = "面  面.FBX";
  std::vector<std::string_view> args{dummyArg0, cjkFile};
  CHECK_EQ(u8toexe(beecli::readCliArgs(args)->inputFile), cjkFile);
}
}

{ // Output
  const auto outFile = "666面 面.gltf"s;
  CHECK_EQ(u8toexe(read_cli_args_with_dummy_and("--out=" + outFile)->outFile),
           outFile);
}

{ // Unit conversion
  CHECK_EQ(read_cli_args_with_dummy_and("--unit-conversion=geometry-level"sv)
               ->convertOptions.unitConversion,
           bee::ConvertOptions::UnitConversion::geometryLevel);

  CHECK_EQ(read_cli_args_with_dummy_and("--unit-conversion=hierarchy-level"sv)
               ->convertOptions.unitConversion,
           bee::ConvertOptions::UnitConversion::hierarchyLevel);

  CHECK_EQ(read_cli_args_with_dummy_and("--unit-conversion=disabled"sv)
               ->convertOptions.unitConversion,
           bee::ConvertOptions::UnitConversion::disabled);
}

{ // No flip V
  CHECK_EQ(
      read_cli_args_with_dummy_and("--no-flip-v"sv)->convertOptions.noFlipV,
      true);

  CHECK_EQ(read_cli_args_with_dummy_and("--no-flip-v=true"sv)
               ->convertOptions.noFlipV,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--no-flip-v=false"sv)
               ->convertOptions.noFlipV,
           false);
}

{ // No texture resolution
  CHECK_EQ(read_cli_args_with_dummy_and("--no-texture-resolution"sv)
               ->convertOptions.textureResolution.disabled,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--no-texture-resolution=true"sv)
               ->convertOptions.textureResolution.disabled,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--no-texture-resolution=false"sv)
               ->convertOptions.textureResolution.disabled,
           false);
}

{// Texture search locations
 {const auto args =
      read_cli_args_with_dummy_and("--texture-search-locations=a"sv);
CHECK_EQ(args->convertOptions.textureResolution.locations.size(), 1);
CHECK_EQ(u8toexe(args->convertOptions.textureResolution.locations[0]), "/a"s);
}

{
  const auto args =
      read_cli_args_with_dummy_and("--texture-search-locations=a,b"sv);
  CHECK_EQ(args->convertOptions.textureResolution.locations.size(), 2);
  CHECK_EQ(args->convertOptions.textureResolution.locations[0], u8"/a"s);
  CHECK_EQ(args->convertOptions.textureResolution.locations[1], u8"/b"s);
}
}

{ // Prefer local time span
  CHECK_EQ(read_cli_args_with_dummy_and("--prefer-local-time-span"sv)
               ->convertOptions.prefer_local_time_span,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--prefer-local-time-span=true"sv)
               ->convertOptions.prefer_local_time_span,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--prefer-local-time-span=false"sv)
               ->convertOptions.prefer_local_time_span,
           false);
}
{
  CHECK_EQ(read_cli_args_with_dummy_and("--match-mesh-names"sv)
               ->convertOptions.match_mesh_names,
           true);
  CHECK_EQ(read_cli_args_with_dummy_and("--match-mesh-names=true"sv)
               ->convertOptions.match_mesh_names,
           true);
  CHECK_EQ(read_cli_args_with_dummy_and("--match-mesh-names=false"sv)
               ->convertOptions.match_mesh_names,
           false);
}

{ // Animation Bake Rate
  CHECK_EQ(read_cli_args_with_dummy_and("--animation-bake-rate=30"sv)
               ->convertOptions.animationBakeRate,
           30);
}

{ // Animation position error mutiplier
  CHECK_EQ(read_cli_args_with_dummy_and(
               "--animation-position-error-multiplier=0.666"sv)
               ->convertOptions.animation_position_error_multiplier,
           doctest::Approx(0.666));
}

{ // Animation scale error mutiplier
  CHECK_EQ(
      read_cli_args_with_dummy_and("--animation-scale-error-multiplier=0.666"sv)
          ->convertOptions.animation_scale_error_multiplier,
      doctest::Approx(0.666));
}

{ // --export-fbx-file-header-info
  CHECK_EQ(read_cli_args_with_dummy_and("--export-fbx-file-header-info"sv)
               ->convertOptions.export_fbx_file_header_info,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--export-fbx-file-header-info=true"sv)
               ->convertOptions.export_fbx_file_header_info,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--export-fbx-file-header-info=false"sv)
               ->convertOptions.export_fbx_file_header_info,
           false);
}

{ // --export-raw-materials
  CHECK_EQ(read_cli_args_with_dummy_and("--export-raw-materials"sv)
               ->convertOptions.export_raw_materials,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--export-raw-materials=true"sv)
               ->convertOptions.export_raw_materials,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--export-raw-materials=false"sv)
               ->convertOptions.export_raw_materials,
           false);
}

{ // Log file
  const auto fbmDir = "666面 面"s;
  CHECK_EQ(u8toexe(read_cli_args_with_dummy_and("--fbm-dir=" + fbmDir)->fbmDir),
           fbmDir);
}

{ // Verbose
  CHECK_EQ(read_cli_args_with_dummy_and("--verbose"sv)->convertOptions.verbose,
           true);

  CHECK_EQ(
      read_cli_args_with_dummy_and("--verbose=true"sv)->convertOptions.verbose,
      true);

  CHECK_EQ(
      read_cli_args_with_dummy_and("--verbose=false"sv)->convertOptions.verbose,
      false);
}

{ // Log file
  const auto logFile = "666.txt"s;
  CHECK_EQ(
      u8toexe(*read_cli_args_with_dummy_and("--log-file=" + logFile)->logFile),
      logFile);
}
}