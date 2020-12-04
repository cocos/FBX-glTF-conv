
#pragma once

// https://github.com/nlohmann/json/issues/1408

#if defined(_MSC_VER)
#undef snprintf
#endif
#include <nlohmann/json.hpp>