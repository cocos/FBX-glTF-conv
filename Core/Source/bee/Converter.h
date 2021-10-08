
#pragma once

#include <bee/BEE_API.h>
#include <bee/polyfills/json.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bee {
class GLTFWriter {
public:
  virtual std::optional<std::u8string> buffer(const std::byte *data_,
                                              std::size_t size_,
                                              std::uint32_t index_,
                                              bool multi_) {
    return {};
  }
};

using Json = nlohmann::json;

class Logger {
public:
  enum class Level {
    verbose,
    info,
    warning,
    error,
    fatal,
  };

  virtual ~Logger() = default;

  virtual void operator()(Level level_, Json &&message_) = 0;

  virtual void operator()(Level level_, std::u8string_view message_) = 0;

  inline void operator()(Level level_, const char8_t *message_) {
    return (*this)(level_, std::u8string_view{message_});
  }

  inline void operator()(Level level_, const std::u8string &message_) {
    return (*this)(level_,
                   std::u8string_view{message_.data(), message_.size()});
  }
};

struct ConvertOptions {
  enum class UnitConversion {
    disabled,
    hierarchyLevel,
    geometryLevel,
  };

  std::u8string out;

  GLTFWriter *writer = nullptr;

  std::optional<std::u8string_view> fbmDir;

  bool useDataUriForBuffers = true;

  UnitConversion unitConversion = UnitConversion::geometryLevel;

  bool noFlipV = false;

  /// <summary>
  /// 0 means auto-detect.
  /// </summary>
  std::uint32_t animationBakeRate = 0;

  struct TextureResolution {
    bool disabled = false;
    std::vector<std::u8string> locations;
  } textureResolution;

  enum class PathMode {
    /// <summary>
    /// Uses relative paths for files which are in a subdirectory of the exported location, absolute for any directories outside that.
    /// </summary>
    prefer_relative,

    /// <summary>
    /// Uses relative paths.
    /// </summary>
    relative,

    /// <summary>
    /// Uses full paths.
    /// </summary>
    absolute,

    /// <summary>
    /// Only write the filename and omit the path component.
    /// </summary>
    strip,

    /// <summary>
    /// Copy the file into output folder and reference using relative path.
    /// </summary>
    copy,

    /// <summary>
    /// Embed the file.
    /// </summary>
    embedded,
  };

  PathMode pathMode = PathMode::prefer_relative;

  bool export_skin = true;

  bool export_blend_shape = true;

  bool export_trs_animation = true;

  bool export_blend_shape_animation = true;

  Logger *logger = nullptr;

  bool verbose = false;
};

Json BEE_API convert(std::u8string_view file_, const ConvertOptions &options_);

} // namespace bee