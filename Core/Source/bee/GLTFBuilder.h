
#pragma once

#include <cstddef>
#include <fx/gltf.h>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace bee {
class GLTFBuilder {
public:
  using XXIndex = std::uint32_t;

  using NoImageAttach = std::monostate;

  struct EmbeddedImage {
    std::string mimeType;
    std::vector<std::byte> data;
  };

  using ImageData = std::variant<NoImageAttach, std::string, EmbeddedImage>;

  struct BufferViewInfo {
    std::byte *data;
    XXIndex index;
  };

  GLTFBuilder();

  struct BuildOptions {
    std::optional<std::string> copyright;
    std::optional<std::string> generator;
  };

  struct BuildResult {
    std::vector<std::vector<std::byte>> buffers;
  };

  fx::gltf::Document &document() {
    return _glTFDocument;
  }

  BuildResult build(BuildOptions options = {});

  const BufferViewInfo createBufferView(std::uint32_t byte_length_,
                                        std::uint32_t align_,
                                        XXIndex buffer_);

  template <fx::gltf::Accessor::Type Type_,
            fx::gltf::Accessor::ComponentType ComponentType_,
            typename Spreader_>
  XXIndex createAccessor(std::span<const typename Spreader_::type> values_,
                         std::uint32_t align_,
                         std::uint32_t buffer_index_,
                         bool min_max_ = false) {
    using SourceTy = typename Spreader_::type;
    using TargetTy = GLTFComponentTypeStorage<ComponentType_>;

    constexpr auto nComponents = countComponents(Type_);
    static_assert(Spreader_::size == nComponents);

    auto [bufferViewData, bufferViewIndex] =
        createBufferView(countBytes(ComponentType_) * nComponents *
                             static_cast<std::uint32_t>(values_.size()),
                         align_, buffer_index_);
    for (decltype(values_.size()) i = 0; i < values_.size(); ++i) {
      Spreader_::spread(values_[i],
                        reinterpret_cast<TargetTy *>(bufferViewData) +
                            nComponents * i);
    }

    fx::gltf::Accessor glTFAccessor;
    glTFAccessor.bufferView = bufferViewIndex;
    glTFAccessor.count = static_cast<std::uint32_t>(values_.size());
    glTFAccessor.type = Type_;
    glTFAccessor.componentType = ComponentType_;
    if (min_max_) {
      glTFAccessor.min.resize(nComponents);
      glTFAccessor.max.resize(nComponents);
      for (std::remove_const_t<decltype(nComponents)> iComp = 0;
           iComp < nComponents; ++iComp) {
        auto minVal = std::numeric_limits<float>::max();
        auto maxVal = -std::numeric_limits<float>::max();
        for (decltype(values_.size()) iVal = iComp; iVal < values_.size();
             iVal += nComponents) {
          const auto val = static_cast<float>(reinterpret_cast<TargetTy *>(
              bufferViewData)[nComponents * iVal + iComp]);
          minVal = std::min(minVal, val);
          maxVal = std::max(maxVal, val);
        }
        glTFAccessor.min[iComp] = minVal;
        glTFAccessor.max[iComp] = maxVal;
      }
    }

    auto glTFAccessorIndex =
        add(&fx::gltf::Document::accessors, std::move(glTFAccessor));
    return glTFAccessorIndex;
  }

  void useExtension(std::string_view extension_name_);

  template <typename T> struct GLTFDocumentMemberPtrValueType {};
  template <typename T>
  struct GLTFDocumentMemberPtrValueType<T fx::gltf::Document::*> {
    using type = typename T::value_type;
  };

  template <
      typename MemberPtr,
      typename = std::enable_if_t<std::is_member_object_pointer_v<MemberPtr>>>
  XXIndex add(MemberPtr where_,
              typename GLTFDocumentMemberPtrValueType<MemberPtr>::type what_) {
    auto &set = _glTFDocument.*where_;
    auto index = static_cast<XXIndex>(set.size());
    set.push_back(std::move(what_));
    return index;
  }

  template <
      typename MemberPtr,
      typename = std::enable_if_t<std::is_member_object_pointer_v<MemberPtr>>>
  decltype(auto) get(MemberPtr where_) {
    return _glTFDocument.*where_;
  }

private:
  struct BufferViewKeep {
    std::size_t index;
    std::size_t align;
    std::vector<std::byte> data;
  };

  struct BufferKeep {
    std::list<BufferViewKeep> bufferViews;
  };

  fx::gltf::Document _glTFDocument;
  std::vector<BufferKeep> _bufferKeeps;
  std::list<ImageData> _images;
};
} // namespace bee