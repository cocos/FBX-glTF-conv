
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

  bool glb = false;

  bool useDataUriForBuffers = true;

  UnitConversion unitConversion = UnitConversion::geometryLevel;

  bool noFlipV = false;

  /// <summary>
  /// 0 means auto-detect.
  /// </summary>
  std::uint32_t animationBakeRate = 0;

  /// <summary>
  /// Whether to prefer local time spans recorded in FBX file for animation exporting.
  /// </summary>
  bool prefer_local_time_span = true;

  bool match_mesh_names = true;

  /// <summary>
  /// Whether to disable mesh instancing.
  /// </summary>
  /// <default>
  /// false
  /// </default>
  bool no_mesh_instancing = false;

  float animation_position_error_multiplier = 1e-5f;

  float animation_scale_error_multiplier = 1e-5f;

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

  bool export_fbx_file_header_info = false;

  bool export_raw_materials = false;
};

struct glTF_output {
  Json json;

  std::optional<std::vector<std::byte>> glb_stored_buffer;
};

glTF_output BEE_API convert(std::u8string_view file_,
                            const ConvertOptions &options_);

} // namespace bee