
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/ObjectDestroyer.h>
#include <bee/Convert/fbxsdk/String.h>
#include <bee/Converter.h>
#include <bee/polyfills/filesystem.h>
#include <bee/polyfills/json.h>
#include <cppcodec/base64_default_rfc4648.hpp>
#include <cstring>
#include <fbxsdk.h>
#include <fmt/format.h>
#include <iostream>
#include <map>

namespace bee {
class Converter {
public:
  Converter(const ConvertOptions &options_) {
    _fbxManager = fbxsdk::FbxManager::Create();
    if (!_fbxManager) {
      throw std::runtime_error("Failed to initialize FBX SDK.");
    }

    if (options_.fbmDir) {
      // TODO: use `FBXImporter::SetEmbeddingExtractionFolder`
      std::string fbmDirCStr{options_.fbmDir->data(),
                             options_.fbmDir->data() + options_.fbmDir->size()};
      auto &xRefManager = _fbxManager->GetXRefManager();
      if (!xRefManager.AddXRefProject(
              fbxsdk::FbxXRefManager::sEmbeddedFileProject,
              fbmDirCStr.data())) {
        if (options_.logger) {
          (*options_.logger)(Logger::Level::warning,
                             u8"Failed to set .fbm dir");
        }
      }
    }
  }

  ~Converter() {
    _fbxManager->Destroy();
  }

  glTF_output BEE_API convert(std::u8string_view file_,
                              const ConvertOptions &options_) {
    GLTFBuilder glTFBuilder;

    auto fbxScene = _import(file_, options_, glTFBuilder);
    FbxObjectDestroyer fbxSceneDestroyer{fbxScene};
    SceneConverter sceneConverter{*_fbxManager, *fbxScene, options_, file_,
                                  glTFBuilder};
    sceneConverter.convert();

    GLTFBuilder::BuildOptions buildOptions;
    buildOptions.generator = "FBX-glTF-conv";
    buildOptions.copyright =
        "Copyright (c) 2018-2020 Chukong Technologies Inc.";
    auto glTFBuildResult = glTFBuilder.build(buildOptions);
    auto &glTFDocument = glTFBuilder.document();

    GLTFWriter defaultWriter;
    auto glTFWriter = options_.writer ? options_.writer : &defaultWriter;

    std::optional<std::vector<std::byte>> glbStoredBuffer;
    {
      const auto nBuffers =
          static_cast<std::uint32_t>(glTFDocument.buffers.size());
      for (std::remove_const_t<decltype(nBuffers)> iBuffer = 0;
           iBuffer < nBuffers; ++iBuffer) {
        auto &glTFBuffer = glTFDocument.buffers[iBuffer];
        const auto &bufferData = glTFBuildResult.buffers[iBuffer];
        if (options_.glb && iBuffer == 0) {
          glbStoredBuffer.emplace(bufferData);
          continue;
        }
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

    return {glTFJson, glbStoredBuffer};
  }

private:
  fbxsdk::FbxManager *_fbxManager = nullptr;

  FbxScene *_import(std::u8string_view file_,
                    const ConvertOptions &options_,
                    GLTFBuilder &glTFBuilder_) {
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
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_MODEL_COUNT, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_DEVICE_COUNT, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_CHARACTER_COUNT,
      // true); fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_ACTOR_COUNT,
      // true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_CONSTRAINT_COUNT,
      // true); fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_MEDIA_COUNT,
      // true);

      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_TEMPLATE, true);
      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_PIVOT, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS,
      // true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_CHARACTER, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_CONSTRAINT, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(
      //    IMP_FBX_MERGE_LAYER_AND_TIMEWARP, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_GOBO, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_SHAPE, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_LINK, true);
      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_MATERIAL, true);
      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_TEXTURE, true);
      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_MODEL, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_AUDIO, true);
      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_ANIMATION, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_PASSWORD, true);
      // fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_PASSWORD_ENABLE,
      // true);
      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_CURRENT_TAKE_NAME,
                                                true);
      fbxImporter->GetIOSettings()->SetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA,
                                                true);
    }

    if (options_.export_fbx_file_header_info) {
      const auto fbxFileHeaderInfo = fbxImporter->GetFileHeaderInfo();

      auto &extensionsAndExtras =
          glTFBuilder_.get(&fx::gltf::Document::extensionsAndExtras);
      auto &glTFFBXFileHeaderInfo =
          extensionsAndExtras["extras"]["FBX-glTF-conv"]["fbxFileHeaderInfo"];
      glTFFBXFileHeaderInfo["creator"] =
          fbx_string_to_utf8_checked(fbxFileHeaderInfo->mCreator);
      if (fbxFileHeaderInfo->mCreationTimeStampPresent) {
        const auto creationTimeStamp = fbxFileHeaderInfo->mCreationTimeStamp;
        glTFFBXFileHeaderInfo["creationTimeStamp"] = {
            {"year", creationTimeStamp.mYear},
            {"month", creationTimeStamp.mMonth},
            {"day", creationTimeStamp.mDay},
            {"hour", creationTimeStamp.mHour},
            {"minute", creationTimeStamp.mMinute},
            {"second", creationTimeStamp.mSecond},
            {"millisecond", creationTimeStamp.mMillisecond},
        };
      }
    }

    if (options_.verbose && options_.logger) {
      const auto fbxFileHeaderInfo = fbxImporter->GetFileHeaderInfo();
      const auto major = fbxFileHeaderInfo->mFileVersion / 1000;
      auto minor = fbxFileHeaderInfo->mFileVersion % 1000;
      while (minor != 0 && minor % 10 == 0) {
        minor /= 10;
      }
      (*options_.logger)(Logger::Level::verbose,
                         fmt::format("FBX file version: {}.{}", major, minor));
      const auto creator =
          static_cast<const char *>(fbxFileHeaderInfo->mCreator);
      (*options_.logger)(Logger::Level::verbose,
                         fmt::format("Creator: {}", creator));
      if (fbxFileHeaderInfo->mCreationTimeStampPresent) {
        (*options_.logger)(
            Logger::Level::verbose,
            fmt::format("Creation time: {}-{}-{} {}:{}:{}",
                        fbxFileHeaderInfo->mCreationTimeStamp.mYear,
                        fbxFileHeaderInfo->mCreationTimeStamp.mMonth,
                        fbxFileHeaderInfo->mCreationTimeStamp.mDay,
                        fbxFileHeaderInfo->mCreationTimeStamp.mHour,
                        fbxFileHeaderInfo->mCreationTimeStamp.mMinute,
                        fbxFileHeaderInfo->mCreationTimeStamp.mSecond));
      }
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

glTF_output BEE_API convert(std::u8string_view file_, const ConvertOptions &options_) {
  Converter converter(options_);
  return converter.convert(file_, options_);
}
} // namespace bee
