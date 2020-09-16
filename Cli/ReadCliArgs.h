

#pragma once

#include <bee/Converter.h>
#include <optional>
#include <string>
#include <string_view>

namespace beecli {
struct CliArgs {
  std::string inputFile;
  std::string outFile;
  std::string fbmDir;
  bee::ConvertOptions convertOptions;
};

std::optional<CliArgs> readCliArgs(int argc_, char *argv_[]);
} // namespace beecli