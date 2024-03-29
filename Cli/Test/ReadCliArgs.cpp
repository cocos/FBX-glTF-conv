
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "ReadCliArgs.h"
#include <doctest/doctest.h>
#include <fmt/format.h>
#include <string>

using namespace std::literals;

const auto dummyArg0 = "UnitTest";
const auto dummyInput = "/Input.fbx"sv;

std::string u8toexe(std::u8string_view u8sv_) {
  return std::string{u8sv_.begin(), u8sv_.end()};
}

auto get_as_convert_command(std::optional<beecli::ParsedCommand> &&command_) {
  CHECK(command_.has_value());
  CHECK(std::holds_alternative<beecli::CliArgs>(*command_));
  return std::move(std::get<beecli::CliArgs>(*command_));
}

auto read_cli_args_with_dummy_and(std::span<std::string_view> extras_) {
  std::vector<std::string_view> args{dummyArg0, dummyInput};
  args.insert(args.end(), extras_.begin(), extras_.end());
  return get_as_convert_command(beecli::readCliArgs(args));
}

auto read_cli_args_with_dummy_and(std::string_view extra_) {
  return read_cli_args_with_dummy_and(std::span<std::string_view>{&extra_, 1});
}

template <bool bee::ConvertOptions::*option_member_ptr>
auto test_boolean_arg(std::string_view name_) {
  CHECK_EQ(
      read_cli_args_with_dummy_and(fmt::format("--{}", name_)).convertOptions.*
          option_member_ptr,
      true);

  CHECK_EQ(read_cli_args_with_dummy_and(fmt::format("--{}=true", name_))
                   .convertOptions.*
               option_member_ptr,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and(fmt::format("--{}=false", name_))
                   .convertOptions.*
               option_member_ptr,
           false);
}

TEST_CASE("Read CLI arguments") {

  { // Empty args
    std::vector<std::string_view> args{dummyArg0};
    const auto command = beecli::readCliArgs(args);
    CHECK_UNARY_FALSE(command.has_value());
  }

  { // Help
    for (const auto param : {"--help", "-h"}) {
      std::vector<std::string_view> args{dummyArg0, "--help"};
      const auto command = beecli::readCliArgs(args);
      CHECK(command.has_value());
      CHECK(std::holds_alternative<beecli::HelpCommand>(*command));
      const auto &helpCommand = std::get<beecli::HelpCommand>(*command);
      CHECK(!helpCommand.text.empty());
    }
  }

  { // Version
    for (const auto param : {"--version", "-v"}) {
      std::vector<std::string_view> args{dummyArg0, param};
      const auto command = beecli::readCliArgs(args);
      CHECK(command.has_value());
      CHECK(std::holds_alternative<beecli::VersionCommand>(*command));
    }
  }

  { // Default args
    const auto convertOptions = read_cli_args_with_dummy_and(std::span<std::string_view>{});
    CHECK_EQ(u8toexe(convertOptions.inputFile), dummyInput);
    CHECK_EQ(u8toexe(convertOptions.outFile), ""s);
    CHECK_EQ(u8toexe(convertOptions.fbmDir), "");
    CHECK_EQ(convertOptions.logFile, std::nullopt);
    CHECK_EQ(convertOptions.convertOptions.prefer_local_time_span, true);
    CHECK_EQ(convertOptions.convertOptions.preserve_mesh_instances, true);
    CHECK_EQ(convertOptions.convertOptions.match_mesh_names, true);
    CHECK_EQ(convertOptions.convertOptions.animationBakeRate, 0);
    CHECK_EQ(convertOptions.convertOptions.animation_position_error_multiplier,
             doctest::Approx(1e-5));
    CHECK_EQ(convertOptions.convertOptions.animation_scale_error_multiplier,
             doctest::Approx(1e-5));
    CHECK_EQ(convertOptions.convertOptions.verbose, false);
    CHECK_EQ(convertOptions.convertOptions.noFlipV, false);
    CHECK_EQ(convertOptions.convertOptions.textureResolution.disabled, false);
    CHECK_EQ(convertOptions.convertOptions.unitConversion,
             bee::ConvertOptions::UnitConversion::geometryLevel);
    CHECK_EQ(convertOptions.convertOptions.export_fbx_file_header_info, false);
    CHECK_EQ(convertOptions.convertOptions.export_raw_materials, false);
  }

  {// Input file
   {std::vector<std::string_view> args{dummyArg0, "--prefer-local-time-span"sv,
                                       dummyInput};
  CHECK_EQ(u8toexe(get_as_convert_command(beecli::readCliArgs(args)).inputFile), dummyInput);
}

{
  std::vector<std::string_view> args{dummyArg0, dummyInput,
                                     "--prefer-local-time-span"sv};
  CHECK_EQ(u8toexe(get_as_convert_command(beecli::readCliArgs(args)).inputFile), dummyInput);
}

{
  std::vector<std::string_view> args{dummyArg0, "--prefer-local-time-span"sv,
                                     "--"sv, dummyInput};
  CHECK_EQ(u8toexe(get_as_convert_command(beecli::readCliArgs(args)).inputFile), dummyInput);
}

{ // CJK, spaces
  const auto cjkFile = "��  ��.FBX";
  std::vector<std::string_view> args{dummyArg0, cjkFile};
  CHECK_EQ(u8toexe(get_as_convert_command(beecli::readCliArgs(args)).inputFile), cjkFile);
}
}

{ // Output
  const auto outFile = "666�� ��.gltf"s;
  CHECK_EQ(u8toexe(read_cli_args_with_dummy_and("--out=" + outFile).outFile),
           outFile);
}

{ // Unit conversion
  CHECK_EQ(read_cli_args_with_dummy_and("--unit-conversion=geometry-level"sv)
               .convertOptions.unitConversion,
           bee::ConvertOptions::UnitConversion::geometryLevel);

  CHECK_EQ(read_cli_args_with_dummy_and("--unit-conversion=hierarchy-level"sv)
               .convertOptions.unitConversion,
           bee::ConvertOptions::UnitConversion::hierarchyLevel);

  CHECK_EQ(read_cli_args_with_dummy_and("--unit-conversion=disabled"sv)
               .convertOptions.unitConversion,
           bee::ConvertOptions::UnitConversion::disabled);
}

{ // No flip V
  test_boolean_arg<&bee::ConvertOptions::noFlipV>("no-flip-v");
}

{ // No texture resolution
  CHECK_EQ(read_cli_args_with_dummy_and("--no-texture-resolution"sv)
               .convertOptions.textureResolution.disabled,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--no-texture-resolution=true"sv)
               .convertOptions.textureResolution.disabled,
           true);

  CHECK_EQ(read_cli_args_with_dummy_and("--no-texture-resolution=false"sv)
               .convertOptions.textureResolution.disabled,
           false);
}

{// Texture search locations
 {const auto args =
      read_cli_args_with_dummy_and("--texture-search-locations=a"sv);
CHECK_EQ(args.convertOptions.textureResolution.locations.size(), 1);
CHECK_EQ(u8toexe(args.convertOptions.textureResolution.locations[0]), "/a"s);
}

{
  const auto args =
      read_cli_args_with_dummy_and("--texture-search-locations=a,b"sv);
  CHECK_EQ(args.convertOptions.textureResolution.locations.size(), 2);
  CHECK_EQ(args.convertOptions.textureResolution.locations[0], u8"/a"s);
  CHECK_EQ(args.convertOptions.textureResolution.locations[1], u8"/b"s);
}
}

{ // Prefer local time span
  test_boolean_arg<&bee::ConvertOptions::prefer_local_time_span>(
      "prefer-local-time-span");
}

{ // --preserve-mesh-instances
  test_boolean_arg<&bee::ConvertOptions::preserve_mesh_instances>(
      "preserve-mesh-instances");
}

{ // --match-mesh-names
  test_boolean_arg<&bee::ConvertOptions::match_mesh_names>("match-mesh-names");
}

{ // Animation Bake Rate
  CHECK_EQ(read_cli_args_with_dummy_and("--animation-bake-rate=30"sv)
               .convertOptions.animationBakeRate,
           30);
}

{ // Animation position error mutiplier
  CHECK_EQ(read_cli_args_with_dummy_and(
               "--animation-position-error-multiplier=0.666"sv)
               .convertOptions.animation_position_error_multiplier,
           doctest::Approx(0.666));
}

{ // Animation scale error mutiplier
  CHECK_EQ(
      read_cli_args_with_dummy_and("--animation-scale-error-multiplier=0.666"sv)
          .convertOptions.animation_scale_error_multiplier,
      doctest::Approx(0.666));
}

{ // --export-fbx-file-header-info
  test_boolean_arg<&bee::ConvertOptions::export_fbx_file_header_info>(
      "export-fbx-file-header-info");
}

{ // --export-raw-materials
  test_boolean_arg<&bee::ConvertOptions::export_raw_materials>(
      "export-raw-materials");
}

{ // Log file
  const auto fbmDir = "666�� ��"s;
  CHECK_EQ(u8toexe(read_cli_args_with_dummy_and("--fbm-dir=" + fbmDir).fbmDir),
           fbmDir);
}

{ // Verbose
  test_boolean_arg<&bee::ConvertOptions::verbose>("verbose");
}

{ // Log file
  const auto logFile = "666.txt"s;
  CHECK_EQ(
      u8toexe(*read_cli_args_with_dummy_and("--log-file=" + logFile).logFile),
      logFile);
}
}