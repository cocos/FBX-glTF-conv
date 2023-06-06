

#include "ReadCliArgs.h"
#include <cxxopts.hpp>
#include <iostream>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <algorithm>
#include <bee/polyfills/filesystem.h>
#include <fmt/format.h>
#include <optional>

namespace beecli {
/// <summary>
/// A core rule is to use UTF-8 across entire application.
/// Command line is one of the place that may produce non-UTF-8 strings.
/// Because the argument strings in `main(argc, argv)` may not be encoded as UTF-8.
/// For example, on Windows, is determinated by the console's code page.
/// This function converts all argument strings into UTF-8 to avoid the encoding issue as possible.
/// For more on "encoding of argv"
/// see https://stackoverflow.com/questions/5408730/what-is-the-encoding-of-argv .
/// </summary>
std::optional<std::vector<std::string>>
getCommandLineArgsU8(int argc_, const char *argv_[]) {
#ifdef _WIN32
  // We first use the Windows API `CommandLineToArgvW` to convert to encoding of
  // "Wide char". And then continuously use the Windows API
  // `WideCharToMultiByte` to convert to UTF-8.
  auto wideStringToUtf8 = [](const LPWSTR wide_str_) {
    const auto wLen = static_cast<int>(wcslen(wide_str_));
    const auto u8Len =
        WideCharToMultiByte(CP_UTF8, 0, wide_str_, wLen, NULL, 0, NULL, NULL);
    std::string result(u8Len, '\0');
    (void)WideCharToMultiByte(CP_UTF8, 0, wide_str_, wLen, result.data(), u8Len,
                              NULL, NULL);
    return result;
  };
  int nArgs = 0;
  LPWSTR *argsW = nullptr;

  argsW = CommandLineToArgvW(GetCommandLineW(), &nArgs);
  if (argsW == NULL) {
    std::cerr << "CommandLineToArgvW failed";
    return {};
  }

  std::vector<std::string> u8Args(nArgs);
  for (decltype(nArgs) iArg = 0; iArg < nArgs; ++iArg) {
    const auto argW = argsW[iArg];
    u8Args[iArg] = wideStringToUtf8(argW);
  }

  LocalFree(argsW);

  return u8Args;
#else
  // On other platforms we may also need to process.
  // But for now we just trust they have already been UTF-8.
  std::vector<std::string> u8Args(argc_);
  for (decltype(argc_) iArg = 0; iArg < argc_; ++iArg) {
    u8Args[iArg] = std::string(argv_[iArg]);
  }
  return u8Args;
#endif
}

template <auto MemberPtr> struct ConvertOptionBindingTrait {};

template <>
struct ConvertOptionBindingTrait<
    &bee::ConvertOptions::animation_position_error_multiplier> {
  constexpr static auto name = "animation-position-error-multiplier";
  constexpr static auto description = "Animation position error multiplier.";
  constexpr static auto default_value = "1e-5";
};

template <>
struct ConvertOptionBindingTrait<
    &bee::ConvertOptions::animation_scale_error_multiplier> {
  constexpr static auto name = "animation-scale-error-multiplier";
  constexpr static auto description = "Animation scale error multiplier.";
  constexpr static auto default_value = "1e-5";
};

template <auto memberPtr> struct convert_option_binding_helper {};

template <typename OptionType, OptionType bee::ConvertOptions::*optionMemberPtr>
struct convert_option_binding_helper<optionMemberPtr> {
  constexpr static auto add_cxx_option(cxxopts::Options &cxx_options_,
                                       bee::ConvertOptions &convert_options_) {
    using Trait = ConvertOptionBindingTrait<optionMemberPtr>;

    cxx_options_.add_options()(std::string{Trait::name},
                               std::string{Trait::description},
                               cxxopts::value<OptionType>()->default_value(
                                   std::string{Trait::default_value}));
  }

  constexpr static auto
  fetch_convert_option(const cxxopts::ParseResult &cxxopts_parse_result_,
                       bee::ConvertOptions &convert_options_) {
    using Trait = ConvertOptionBindingTrait<optionMemberPtr>;
    if (cxxopts_parse_result_.count(Trait::name)) {
      convert_options_.*optionMemberPtr =
          cxxopts_parse_result_[Trait::name].template as<OptionType>();
    }
  }
};

template <auto memberPtr>
constexpr static auto add_cxx_option(cxxopts::Options &cxx_options_,
                                     bee::ConvertOptions &convert_options_) {
  return convert_option_binding_helper<memberPtr>::add_cxx_option(
      cxx_options_, convert_options_);
}

template <auto memberPtr>
constexpr static auto
fetch_convert_option(const cxxopts::ParseResult &cxxopts_parse_result_,
                     bee::ConvertOptions &convert_options_) {
  return convert_option_binding_helper<memberPtr>::fetch_convert_option(
      cxxopts_parse_result_, convert_options_);
}

std::optional<CliArgs> readCliArgs(std::span<std::string_view> args_) {
  std::string inputFile;
  std::string outFile;
  std::string fbmDir;
  std::string logFile;
  std::string unitConversion;
  std::vector<std::string> textureSearchLocations;

  const std::array<std::u8string_view, 2> tslMacros = {u8"cwd",
                                                       u8"fileDirName"};

  CliArgs cliArgs;

  cxxopts::Options options{"FBX-glTF-conv",
                           "This is a FBX to glTF file format converter."};

  const auto add_cxx_option = [&options, &cliArgs ]<auto memberPtr>() {
    convert_option_binding_helper<memberPtr>::add_cxx_option(
        options, cliArgs.convertOptions);
  };

  options.add_options()("input-file", "Input file",
                        cxxopts::value<std::string>());

  options.add_options()("fbm-dir", "The directory to store the embedded media.",
                        cxxopts::value<std::string>());
  options.add_options()(
      "out",
      "The output path to the .gltf or .glb file. Defaults to "
      "`<working-directory>/<FBX-filename-basename>.gltf`",
      cxxopts::value<std::string>());
  options.add_options()("no-flip-v", "Do not flip V texture coordinates.",
                        cxxopts::value<bool>()->default_value("false"));
  options.add_options()(
      "unit-conversion",
      "How to perform unit converseion.\n"
      "  - `geometry-level` Do unit conversion at "
      "geometry "
      "level.\n"
      "  - `hierarchy-level` Do unit conversion at hierarchy "
      "level.\n"
      "  - `disabled` Disable unit conversion. This may cause the "
      "generated glTF does't conform to glTF specification.",
      cxxopts::value<std::string>()->default_value("geometry-level"));

  options.add_options()("no-texture-resolution", "Do not resolve textures.",
                        cxxopts::value<bool>()->default_value("false"));

  options.add_options()(
      "prefer-local-time-span",
      "Prefer local time spans recorded in FBX file for animation "
      "exporting.",
      cxxopts::value<bool>()->default_value("true"));
  options.add_options()(
      "match-mesh-names",
      "Prefer mesh names "
      "exporting.",
      cxxopts::value<bool>()->default_value("true"));

  options.add_options()(
      "animation-bake-rate", "Animation bake rate(in FPS).",
      cxxopts::value<decltype(cliArgs.convertOptions.animationBakeRate)>()
          ->default_value("30"));

  add_cxx_option.template
  operator()<&bee::ConvertOptions::animation_position_error_multiplier>();

  add_cxx_option.template
  operator()<&bee::ConvertOptions::animation_scale_error_multiplier>();

  options.add_options()(
      "texture-search-locations",
      "Texture search locations. These path shall be absolute "
      "path or relative path from input file's directory.",
      cxxopts::value<std::vector<std::string>>());

  options.add_options()("export-fbx-file-header-info",
                        "Export FBX file header info.",
                        cxxopts::value<bool>()->default_value("false"));

  options.add_options()("export-raw-materials", "Export raw materials.",
                        cxxopts::value<bool>()->default_value("false"));

  options.add_options()("verbose", "Verbose output.",
                        cxxopts::value<bool>()->default_value("false"));
  options.add_options()(
      "log-file",
      "Specify the log file(logs are outputed as JSON). If not "
      "specified, logs're printed to "
      "console",
      cxxopts::value<std::string>());

  options.add_options()(
      "image-path-mode",
      "Specify the mode used to specify the image path. Could "
      "be one of the following:\n"
      "- absolute - Output the absolute path to the path.\n"
      "- relative - Output the relative path to the path.\n"
      "- prefer-relative - If the image is under the the same or sub directory "
      "of "
      "glTF file, output as relative. Otherwise output as absolute.\n"
      "- strip - Ingore all path information, only output the file name.\n"
      "- embedded - Embedded the image into Data URI.\n"
      "- copy - Copy the image to the output directory and reference it with a "
      "relative path.\n",
      cxxopts::value<std::string>());

  options.parse_positional("input-file");

  std::vector<std::string> argStrings(args_.size());
  std::transform(args_.begin(), args_.end(), argStrings.begin(),
                 [](auto &p) { return p.data(); });

  std::vector<char *> argsMutable(argStrings.size());
  std::transform(argStrings.begin(), argStrings.end(), argsMutable.begin(),
                 [](auto &p) { return p.data(); });

  auto argc = static_cast<int>(argsMutable.size());
  auto argv = argsMutable.data();

  try {
    const auto cliParseResult = options.parse(argc, argv);

    if (cliParseResult.count("help")) {
      std::cout << options.help() << std::endl;
      return {};
    }

    const auto fetch_convert_option =
        [&cliParseResult, &cliArgs ]<auto memberPtr>() {
      convert_option_binding_helper<memberPtr>::fetch_convert_option(
          cliParseResult, cliArgs.convertOptions);
    };

    if (cliParseResult.count("input-file")) {
      inputFile = cliParseResult["input-file"].as<std::string>();
    }

    if (cliParseResult.count("fbm-dir")) {
      fbmDir = cliParseResult["fbm-dir"].as<std::string>();
    }

    if (cliParseResult.count("out")) {
      outFile = cliParseResult["out"].as<std::string>();
    }

    if (cliParseResult.count("no-flip-v")) {
      cliArgs.convertOptions.noFlipV = cliParseResult["no-flip-v"].as<bool>();
    }

    if (cliParseResult.count("unit-conversion")) {
      unitConversion = cliParseResult["unit-conversion"].as<std::string>();
    }

    if (cliParseResult.count("no-texture-resolution")) {
      cliArgs.convertOptions.textureResolution.disabled =
          cliParseResult["no-texture-resolution"].as<bool>();
    }

    if (cliParseResult.count("texture-search-locations")) {
      textureSearchLocations = cliParseResult["texture-search-locations"]
                                   .as<std::vector<std::string>>();
    }

    if (cliParseResult.count("prefer-local-time-span")) {
      cliArgs.convertOptions.prefer_local_time_span =
          cliParseResult["prefer-local-time-span"].as<bool>();
    }
    if (cliParseResult.count("match-mesh-names")) {
	  cliArgs.convertOptions.match_mesh_names =
		  cliParseResult["match-mesh-names"].as<bool>();
	}
    if (cliParseResult.count("animation-bake-rate")) {
      cliArgs.convertOptions.animationBakeRate =
          cliParseResult["animation-bake-rate"]
              .as<decltype(cliArgs.convertOptions.animationBakeRate)>();
    }

    fetch_convert_option.template
    operator()<&bee::ConvertOptions::animation_position_error_multiplier>();

    fetch_convert_option.template
    operator()<&bee::ConvertOptions::animation_scale_error_multiplier>();

    if (cliParseResult.count("export-fbx-file-header-info")) {
      cliArgs.convertOptions.export_fbx_file_header_info =
          cliParseResult["export-fbx-file-header-info"].as<bool>();
    }

    if (cliParseResult.count("export-raw-materials")) {
      cliArgs.convertOptions.export_raw_materials =
          cliParseResult["export-raw-materials"].as<bool>();
    }

    cliArgs.convertOptions.pathMode = bee::ConvertOptions::PathMode::copy;
    if (cliParseResult.count("image-path-mode")) {
      const auto pathModeString =
          cliParseResult["image-path-mode"].as<std::string>();
      if (pathModeString == "relative") {
        cliArgs.convertOptions.pathMode =
            bee::ConvertOptions::PathMode::relative;
      } else if (pathModeString == "absolute") {
        cliArgs.convertOptions.pathMode =
            bee::ConvertOptions::PathMode::absolute;
      } else if (pathModeString == "prefer-relative") {
        cliArgs.convertOptions.pathMode =
            bee::ConvertOptions::PathMode::prefer_relative;
      } else if (pathModeString == "strip") {
        cliArgs.convertOptions.pathMode = bee::ConvertOptions::PathMode::strip;
      } else if (pathModeString == "embedded") {
        cliArgs.convertOptions.pathMode =
            bee::ConvertOptions::PathMode::embedded;
      } else if (pathModeString == "copy") {
        cliArgs.convertOptions.pathMode = bee::ConvertOptions::PathMode::copy;
      } else {
        std::cerr << "Bad --image-path-mode \"" << pathModeString << "\"\n";
        std::cout << options.help() << std::endl;
        return {};
      }
    }

    if (cliParseResult.count("verbose")) {
      cliArgs.convertOptions.verbose = cliParseResult["verbose"].as<bool>();
    }

    if (cliParseResult.count("log-file")) {
      logFile = cliParseResult["log-file"].as<std::string>();
    }

    if (inputFile.empty()) {
      std::cerr << "Input file not specified." << std::endl;
      std::cerr << options.help() << std::endl;
      return {};
    }
  } catch (const cxxopts::exceptions::exception &ex_) {
    std::cerr << ex_.what() << "\n";
    std::cout << options.help() << std::endl;
    return {};
  }

  if (!unitConversion.empty()) {
    if (unitConversion == "geometry-level") {
      cliArgs.convertOptions.unitConversion =
          bee::ConvertOptions::UnitConversion::geometryLevel;
    } else if (unitConversion == "hierarchy-level") {
      cliArgs.convertOptions.unitConversion =
          bee::ConvertOptions::UnitConversion::hierarchyLevel;
    } else if (unitConversion == "disabled") {
      cliArgs.convertOptions.unitConversion =
          bee::ConvertOptions::UnitConversion::disabled;
    } else {
      std::cerr << "Unknown unit conversion option: " << unitConversion << "\n";
    }
  }

  cliArgs.inputFile.assign(inputFile.begin(), inputFile.end());
  cliArgs.outFile.assign(outFile.begin(), outFile.end());
  cliArgs.fbmDir.assign(fbmDir.begin(), fbmDir.end());
  if (!logFile.empty()) {
    cliArgs.logFile.emplace();
    cliArgs.logFile->assign(logFile.begin(), logFile.end());
  }
  if (!textureSearchLocations.empty()) {
    const auto baseDir = bee::filesystem::path{inputFile}.parent_path();
    cliArgs.convertOptions.textureResolution.locations.resize(
        textureSearchLocations.size());
    std::transform(
        textureSearchLocations.begin(), textureSearchLocations.end(),
        cliArgs.convertOptions.textureResolution.locations.begin(),
        [&baseDir](auto &s_) {
          const auto p = bee::filesystem::path{std::u8string_view{
              reinterpret_cast<const char8_t *>(s_.data()), s_.size()}};
          if (p.is_absolute()) {
            return p.u8string();
          } else {
            return (baseDir / p).u8string();
          }
        });
  }

  return cliArgs;
}
} // namespace beecli