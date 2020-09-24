

#include "ReadCliArgs.h"
#include <type_traits>

// https://github.com/muellan/clipp/issues/53
// clang is not completed: https://libcxx.llvm.org/cxx2a_status.html
#if _MSC_VER
namespace std {
template <class> struct result_of;
template <class F, class... ArgTypes>
struct result_of<F(ArgTypes...)> : std::invoke_result<F, ArgTypes...> {};
} // namespace std
#endif
#include <clipp.h>

#include <iostream>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <optional>

/// <summary>
/// A core rule is to use UTF-8 across entire application.
/// Command line is one of the place that may produce non-UTF-8 strings.
/// Because the argument strings in `main(argc, argv)` may not be encoded as UTF-8.
/// For example, on Windows, is determinated by the console's code page.
/// This function converts all argument strings into UTF-8 to avoid the encoding issue as possible.
/// For more on "encoding of argv"
/// see https://stackoverflow.com/questions/5408730/what-is-the-encoding-of-argv .
/// </summary>
std::optional<std::vector<std::string>> getCommandLineArgsU8(int argc_,
                                                             char *argv_[]) {
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

namespace beecli {
std::optional<CliArgs> readCliArgs(int argc_, char *argv_[]) {
  std::string inputFile;
  std::string outFile;
  std::string fbmDir;

  CliArgs cliArgs;
  auto cli = (

      clipp::value("input file", inputFile),

      clipp::option("--out").set(outFile).doc(
          "The output path to the .gltf or .glb file. Defaults to "
          "`<working-directory>/<FBX-filename-basename>.gltf`"),

      clipp::option("--fbm-dir")
          .set(fbmDir)
          .doc("The directory to store the embedded media."),

      clipp::option("--no-flip-v")
          .set(cliArgs.convertOptions.noFlipV)
          .doc("Do not flip V texture coordinates."),

      clipp::option("--animation-bake-rate")
          .set(cliArgs.convertOptions.animationBakeRate)
          .doc("Animation bake rate(in FPS)."),

      clipp::option("--suspected-animation-duration-limit")
          .set(cliArgs.convertOptions.suspectedAnimationDurationLimit)
          .doc("The suspected animation duration limit."),

      clipp::option("--verbose")
          .set(cliArgs.convertOptions.verbose)
          .doc("Verbose output.")

  );

  const auto commandLineArgsU8 = getCommandLineArgsU8(argc_, argv_);
  if (!commandLineArgsU8) {
    return {};
  }

  // clipp: only `parse(argc, argv, cli)` form will automaticly exclude the
  // first arg We should manually do here. See
  // https://github.com/muellan/clipp#parsing
  if (commandLineArgsU8->empty() ||
      !clipp::parse(commandLineArgsU8->begin() + 1, commandLineArgsU8->end(),
                    cli)) {
    std::cout << make_man_page(
        cli, commandLineArgsU8->empty() ? "" : commandLineArgsU8->front());
    return {};
  }

  cliArgs.inputFile.assign(inputFile.begin(), inputFile.end());
  cliArgs.outFile.assign(outFile.begin(), outFile.end());
  cliArgs.fbmDir.assign(fbmDir.begin(), fbmDir.end());

  return cliArgs;
}
} // namespace beecli