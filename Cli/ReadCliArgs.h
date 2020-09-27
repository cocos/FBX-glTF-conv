

#pragma once

#include <bee/Converter.h>
#include <optional>
#include <string>
#include <string_view>

namespace beecli {
struct CliArgs {
  std::u8string inputFile;
  std::u8string outFile;
  std::u8string fbmDir;
  std::optional<std::u8string> logFile;
  bee::ConvertOptions convertOptions;
};

std::optional<CliArgs> readCliArgs(int argc_, char *argv_[]);
} // namespace beecli