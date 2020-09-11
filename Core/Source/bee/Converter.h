
#pragma once

#include <optional>
#include <bee/BEE_API.h>
#include <string>
#include <string_view>

namespace bee {
class GLTFWriter {
public:
  virtual std::optional<std::string> buffer(const std::byte *data_,
                                            std::size_t size_,
                                            std::uint32_t index_,
                                            bool multi_) {
    return {};
  }
};

struct ConvertOptions {
  std::string out;

  GLTFWriter *writer = nullptr;

  std::optional<std::string_view> fbmDir;

  bool useDataUriForBuffers = true;

  bool noFlipV = false;

  std::uint32_t animationBakeRate = 30;

  std::uint32_t suspectedAnimationDurationLimit =
      60 * 10; // I think 10 minutes is extraordinary enough...

  struct TextureSearch {
    std::vector<std::string> locations;
  } textureSearch;

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
};

std::string BEE_API convert(std::string_view file_,
                             const ConvertOptions &options_);

} // namespace bee