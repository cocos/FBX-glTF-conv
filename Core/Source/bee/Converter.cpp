
#include <cstring>
#include <fbxsdk.h>
#include <iostream>
#include <map>
#if defined(_MSC_VER)
#undef snprintf
#endif
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/ObjectDestroyer.h>
#include <bee/Converter.h>
#include <bee/polyfills/filesystem.h>
#include <cppcodec/base64_default_rfc4648.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace bee {
class Converter {
public:
  Converter(const ConvertOptions &options_) {
    _fbxManager = fbxsdk::FbxManager::Create();
    if (!_fbxManager) {
      throw std::runtime_error("Failed to initialize FBX SDK.");
    }

    if (options_.fbmDir) {
      std::string fbmDirCStr{options_.fbmDir->data(),
                             options_.fbmDir->data() + options_.fbmDir->size()};
      auto &xRefManager = _fbxManager->GetXRefManager();
      if (!xRefManager.AddXRefProject(
              fbxsdk::FbxXRefManager::sEmbeddedFileProject,
              fbmDirCStr.data())) {
        _warn("Failed to set .fbm dir");
      }
    }
  }

  ~Converter() {
    _fbxManager->Destroy();
  }

  std::string BEE_API convert(std::u8string_view file_,
                              const ConvertOptions &options_) {
    auto fbxScene = _import(file_);
    FbxObjectDestroyer fbxSceneDestroyer{fbxScene};
    GLTFBuilder glTFBuilder;
    SceneConverter sceneConverter{*_fbxManager, *fbxScene, options_, file_,
                                  glTFBuilder};
    sceneConverter.convert();

    GLTFBuilder::BuildOptions buildOptions;
    buildOptions.generator = "Cocos FBX to glTF";
    buildOptions.copyright =
        "Copyright (c) 2018-2020 Chukong Technologies Inc.";
    auto glTFBuildResult = glTFBuilder.build(buildOptions);
    auto &glTFDocument = glTFBuilder.document();

    GLTFWriter defaultWriter;
    auto glTFWriter = options_.writer ? options_.writer : &defaultWriter;

    {
      const auto nBuffers =
          static_cast<std::uint32_t>(glTFDocument.buffers.size());
      for (std::remove_const_t<decltype(nBuffers)> iBuffer = 0;
           iBuffer < nBuffers; ++iBuffer) {
        auto &glTFBuffer = glTFDocument.buffers[iBuffer];
        const auto &bufferData = glTFBuildResult.buffers[iBuffer];
        std::optional<std::string> uri;
        if (!options_.useDataUriForBuffers) {
          auto u8Uri = glTFWriter->buffer(bufferData.data(), bufferData.size(),
                                          iBuffer, nBuffers != 1);
          if (u8Uri) {
            uri = std::string{u8Uri->begin(), u8Uri->end()};
          }
        }
        if (!uri) {
          auto base64Data = cppcodec::base64_rfc4648::encode(
              reinterpret_cast<const char *>(bufferData.data()),
              bufferData.size());
          uri = "data:application/octet-stream;base64," + base64Data;
        }
        glTFBuffer.uri = *uri;
      }
    }

    {
      const auto nImages = glTFDocument.images.size();
      for (std::remove_const_t<decltype(nImages)> iImage = 0; iImage < nImages;
           ++iImage) {
      }
    }

    nlohmann::json glTFJson;
    fx::gltf::to_json(glTFJson, glTFDocument);

    return glTFJson.dump(2);
  }

private:
  fbxsdk::FbxManager *_fbxManager = nullptr;

  void _warn(std::string_view message_) {
    std::cout << message_ << "\n";
  }

  FbxScene *_import(std::u8string_view file_) {
    auto ioSettings = fbxsdk::FbxIOSettings::Create(_fbxManager, IOSROOT);
    _fbxManager->SetIOSettings(ioSettings);

    auto fbxImporter = fbxsdk::FbxImporter::Create(_fbxManager, "");
    FbxObjectDestroyer fbxImporterDestroyer{fbxImporter};

    auto inputFileCStr = std::string{file_.data(), file_.data() + file_.size()};
    auto importInitOk = fbxImporter->Initialize(inputFileCStr.c_str(), -1,
                                                _fbxManager->GetIOSettings());
    if (!importInitOk) {
      const auto status = fbxImporter->GetStatus();
      throw std::runtime_error("Failed to initialize FBX importer: " +
                               std::string() + status.GetErrorString());
    }

    if (fbxImporter->IsFBX()) {
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_MODEL, true);
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_MATERIAL, true);
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_TEXTURE, true);
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_EMBEDDED, true);
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_SHAPE, true);
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_GOBO, true);
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_ANIMATION, true);
      fbxImporter->GetIOSettings()->SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);
    }

    auto fbxScene = fbxsdk::FbxScene::Create(_fbxManager, "");
    auto importOk = fbxImporter->Import(fbxScene);
    if (!importOk) {
      const auto status = fbxImporter->GetStatus();
      throw std::runtime_error("Failed to import scene." + std::string() +
                               status.GetErrorString());
    }

    return fbxScene;
  }
};

std::string BEE_API convert(std::u8string_view file_,
                            const ConvertOptions &options_) {
  Converter converter(options_);
  return converter.convert(file_, options_);
}
} // namespace bee
