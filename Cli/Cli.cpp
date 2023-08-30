
#include "ReadCliArgs.h"
#include "Version.h"
#include <array>
#include <bee/Converter.h>
#include <bee/polyfills/filesystem.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>

std::u8string relativeUriBetweenPath(const bee::filesystem::path &from_,
                                     const bee::filesystem::path &to_) {
  return to_.lexically_relative(from_).generic_u8string();
}

class ConsoleLogger : public bee::Logger {
public:
  void operator()(Level level_, bee::Json &&message_) override {
    const auto text = message_.dump(2);
    (*this)(level_,
            std::u8string_view{reinterpret_cast<const char8_t *>(text.data()),
                               text.size()});
  }

  void operator()(Level level_, std::u8string_view message_) override {
    auto &stream = level_ >= bee::Logger::Level::error ? std::cerr : std::cout;
    stream << std::string{reinterpret_cast<const char *>(message_.data()),
                          message_.size()}
           << "\n";
  }
};

class JsonLogger : public bee::Logger {
public:
  void operator()(Level level_, bee::Json &&message_) override {
    _messages.push_back(bee::Json{{"level", level_}, {"message", message_}});
  }

  void operator()(Level level_, std::u8string_view message_) override {
    (*this)(level_, bee::Json(std::string_view{
                        reinterpret_cast<const char *>(message_.data()),
                        message_.size()}));
  }

  const bee::Json &messages() const {
    return _messages;
  }

private:
  bee::Json _messages = bee::Json::array();
};

std::vector<std::byte>
make_glb(std::string_view json_text_,
         std::optional<std::span<const std::byte>> glb_stored_buffer_) {
  const auto align = [&](std::uint32_t size_, std::uint32_t as_) {
    assert(as_ > 0);
    const auto remainder = size_ % as_;
    if (remainder != 0) {
      size_ += (as_ - remainder);
    }
    return size_;
  };

  const auto jsonTextSizeAligned =
      align(static_cast<std::uint32_t>(json_text_.size()), 4);
  const auto glbStoredBufferSizeAligned =
      align(static_cast<std::uint32_t>(glb_stored_buffer_->size()), 4);

  // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-gltf-layout
  const auto glbSize =
      (4 + 4 + 4) + (4 + 4 + jsonTextSizeAligned) +
      (glb_stored_buffer_ ? (4 + 4 + glbStoredBufferSizeAligned) : 0);

  std::vector<std::byte> glb(glbSize, static_cast<std::byte>(0));
  std::vector<std::byte>::size_type p = 0;

  const auto writeU32 = [&](std::uint32_t v_) {
    *reinterpret_cast<std::uint32_t *>(glb.data() + p) = v_;
    p += 4;
  };

  writeU32(0x46546C67u);                         // magic
  writeU32(2u);                                  // version
  writeU32(static_cast<std::uint32_t>(glbSize)); // total size

  writeU32(jsonTextSizeAligned);
  writeU32(0x4E4F534Au); // chunk type: JSON
  std::transform(json_text_.begin(), json_text_.end(), glb.begin() + p,
                 [](char ch_) { return static_cast<std::byte>(ch_); });
  // >> This chunk MUST be padded with trailing Space chars (0x20) to satisfy
  // >> alignment requirements.
  std::fill_n(glb.begin() + p + json_text_.size(),
              jsonTextSizeAligned - json_text_.size(),
              static_cast<std::byte>(0x20));
  p += jsonTextSizeAligned;

  if (glb_stored_buffer_) {
    writeU32(glbStoredBufferSizeAligned);
    writeU32(0x004E4942u); // chunk type: BIN
    std::copy(glb_stored_buffer_->begin(), glb_stored_buffer_->end(),
              glb.begin() + p);
    // >> This chunk MUST be padded with trailing zeros (0x00) to satisfy
    // >> alignment requirements.
    std::fill_n(glb.begin() + p + glb_stored_buffer_->size(),
                glbStoredBufferSizeAligned - glb_stored_buffer_->size(),
                static_cast<std::byte>(0x0));
    p += glbStoredBufferSizeAligned;
  }

  assert(p == glb.size() && "Pre-allocation calculating error!");

  return glb;
}

int main(int argc_, const char *argv_[]) {
  namespace fs = bee::filesystem;

  const auto argsU8 = beecli::getCommandLineArgsU8(argc_, argv_);
  if (!argsU8) {
    return -1;
  }

  std::vector<std::string_view> argsU8SV(argsU8->size());
  std::transform(argsU8->begin(), argsU8->end(), argsU8SV.begin(),
                 [](auto &s_) {
                   return std::string_view{s_.data(), s_.size()};
                 });

  auto parsedCommand = beecli::readCliArgs(argsU8SV);
  if (!parsedCommand) {
    return -1;
  }

  if (std::holds_alternative<beecli::HelpCommand>(*parsedCommand)) {
    const auto &command = std::get<beecli::HelpCommand>(*parsedCommand);
    std::cout << command.text << std::endl;
    return 0;
  }

  if (std::holds_alternative<beecli::VersionCommand>(*parsedCommand)) {
    const auto &command = std::get<beecli::VersionCommand>(*parsedCommand);
    std::cout << beecli::version_string << std::flush;
    return 0;
  }

  assert(std::holds_alternative<beecli::CliArgs>(*parsedCommand));
  const auto cliOptions = &std::get<beecli::CliArgs>(*parsedCommand);

  if (!cliOptions->fbmDir.empty()) {
    cliOptions->convertOptions.fbmDir = cliOptions->fbmDir;
  }

  if (cliOptions->outFile.empty()) {
    const auto inputFilePath = fs::path{cliOptions->inputFile};
    const auto inputBaseNameNoExt = inputFilePath.stem().string();
    auto outFilePath = fs::current_path() / (inputBaseNameNoExt + "_glTF") /
                       (inputBaseNameNoExt + ".gltf");
    fs::create_directories(outFilePath.parent_path());
    cliOptions->outFile = outFilePath.u8string();
  }
  cliOptions->convertOptions.out = cliOptions->outFile;
  { // Deduce .glb from output path
    const auto extension = fs::path(cliOptions->outFile).extension().string();
    std::string extensionLower = extension;
    std::transform(extensionLower.begin(), extensionLower.end(),
                   extensionLower.begin(), ::tolower);
    if (extensionLower == ".glb") {
      cliOptions->convertOptions.glb = true;
    }
  }

  class MyWriter : public bee::GLTFWriter {
  public:
    MyWriter(std::u8string_view in_file_, std::u8string_view out_file_)
        : _inFile(in_file_), _outFile(out_file_) {
    }

    virtual std::optional<std::u8string> buffer(const std::byte *data_,
                                                std::size_t size_,
                                                std::uint32_t index_,
                                                bool multi_) {
      const auto outFilePath = fs::path{_outFile};
      const auto glTFOutBaseName = outFilePath.stem();
      const auto glTFOutDir = outFilePath.parent_path();
      const auto bufferOutPath =
          glTFOutDir /
          (multi_ ? (glTFOutBaseName.string() + std::to_string(index_) + ".bin")
                  : (glTFOutBaseName.string() + ".bin"));
      std::error_code errc;
      fs::create_directories(bufferOutPath.parent_path(), errc);
      if (errc) {
        throw std::runtime_error("Failed to create directories for buffer " +
                                 bufferOutPath.string());
      }

      std::ofstream ofs(bufferOutPath.string(), std::ios::binary);
      ofs.exceptions(std::ios::badbit | std::ios::failbit);
      ofs.write(reinterpret_cast<const char *>(data_), size_);
      ofs.flush();

      return relativeUriBetweenPath(glTFOutDir, bufferOutPath);
    }

  private:
    std::u8string_view _inFile;
    std::u8string_view _outFile;
  };

  MyWriter writer{cliOptions->inputFile, cliOptions->outFile};
  cliOptions->convertOptions.useDataUriForBuffers = false;
  cliOptions->convertOptions.writer = &writer;

  std::unique_ptr<bee::Logger> logger;
  if (cliOptions->logFile) {
    logger = std::make_unique<JsonLogger>();
  } else {
    logger = std::make_unique<ConsoleLogger>();
  }
  cliOptions->convertOptions.logger = logger.get();

  // `0` means success
  // `1` means error happened but it's captured and logged.
  constexpr int exitOk = 0;
  constexpr int exitFailureCaptured = 1;
  int retval = exitOk;

  try {
    const auto glTFOutput =
        bee::convert(cliOptions->inputFile, cliOptions->convertOptions);

    const auto outFilePath = fs::path{cliOptions->outFile};
    const auto glbOut = cliOptions->convertOptions.glb;
    if (!glbOut) {
      assert(!glTFOutput.glb_stored_buffer &&
             "Should not have GLB stored buffer in such case!");
      std::ofstream glTFJsonOStream(outFilePath.string());
      glTFJsonOStream.exceptions(std::ios::badbit | std::ios::failbit);
      const auto glTFJsonText = glTFOutput.json.dump(2);
      glTFJsonOStream << glTFJsonText;
      glTFJsonOStream.flush();
    } else {
      std::ofstream glTFBinaryOStream(outFilePath.string(), std::ios::binary);
      glTFBinaryOStream.exceptions(std::ios::badbit | std::ios::failbit);
      const auto glTFJsonText = glTFOutput.json.dump(0);
      const auto glb = make_glb(glTFJsonText, glTFOutput.glb_stored_buffer);
      glTFBinaryOStream.write(reinterpret_cast<const char *>(glb.data()),
                              static_cast<std::streamsize>(glb.size()));
      glTFBinaryOStream.flush();
    }
  } catch (const std::exception &exception) {
    logger->operator()(bee::Logger::Level::fatal, exception.what());
    retval = exitFailureCaptured;
  }

  if (cliOptions->logFile) {
    const auto jsonLogger = dynamic_cast<const JsonLogger *>(logger.get());
    assert(jsonLogger);
    try {
      const auto logFilePath = fs::path{*cliOptions->logFile};
      fs::create_directories(logFilePath.parent_path());
      std::ofstream jsonLogOStream{logFilePath};
      jsonLogOStream.exceptions(std::ios::badbit | std::ios::failbit);
      const auto jsonLogText = jsonLogger->messages().dump(2);
      jsonLogOStream << jsonLogText;
    } catch (const std::exception &exception) {
      std::cerr << exception.what() << "\n";
      retval = exitFailureCaptured;
    }
  }

  return retval;
}