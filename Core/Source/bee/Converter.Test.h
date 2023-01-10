#pragma once

#include "./Converter.h"
#include "./GLTFBuilder.h"
#include <string>

namespace bee {
GLTFBuilder BEE_API _convert_test(std::u8string_view file_,
                                  const ConvertOptions &options_);
}