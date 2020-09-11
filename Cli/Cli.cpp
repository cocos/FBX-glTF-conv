
#include <array>
#include <clipp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <bee/Converter.h>
#include <string>

std::string relativeUriBetweenPath(std::filesystem::path &from_,
                                   std::filesystem::path &to_) {
  namespace fs = std::filesystem;
  auto rel = to_.lexically_relative(from_);
  return std::accumulate(rel.begin(), rel.end(), std::string{},
                         [](const std::string &init_, const fs::path &seg_) {
                           return init_ + (init_.size() > 0 ? "/" : "") +
                                  seg_.string();
                         });
}

int main(int argc_, char *argv_[]) {
  namespace fs = std::filesystem;

  std::string inputFile;
  std::string outFile;
  std::string fbmDir;
  bee::ConvertOptions convertOptions;

  auto cli = (

      clipp::value("input file", inputFile),

      clipp::option("--out").set(outFile).doc(
          "The output path to the .gltf or .glb file. Defaults to "
          "`<working-directory>/<FBX-filename-basename>.gltf`"),

      clipp::option("--fbm-dir")
          .set(fbmDir)
          .doc("The directory to store the embedded media."),

      clipp::option("--no-flip-v")
          .set(convertOptions.noFlipV)
          .doc("Do not flip V texture coordinates."),

      clipp::option("--animation-bake-rate")
          .set(convertOptions.animationBakeRate)
          .doc("Animation bake rate(in FPS)."),

      clipp::option("--suspected-animation-duration-limit")
          .set(convertOptions.suspectedAnimationDurationLimit)
          .doc("The suspected animation duration limit.")

  );

  if (!clipp::parse(argc_, argv_, cli)) {
    std::cout << make_man_page(cli, argv_[0]);
    return -1;
  }
  if (!fbmDir.empty()) {
    convertOptions.fbmDir = fbmDir;
  }

  if (outFile.empty()) {
    const auto inputFilePath = fs::path{inputFile};
    const auto inputBaseNameNoExt = inputFilePath.stem().string();
    auto outFilePath = fs::current_path() / (inputBaseNameNoExt + "_glTF") /
                       (inputBaseNameNoExt + ".gltf");
    fs::create_directories(outFilePath.parent_path());
    outFile = outFilePath.string();
  }
  convertOptions.out = outFile;

  class MyWriter : public bee::GLTFWriter {
  public:
    MyWriter(std::string_view in_file_, std::string_view out_file_)
        : _inFile(in_file_), _outFile(out_file_) {
    }

    virtual std::optional<std::string> buffer(const std::byte *data_,
                                              std::size_t size_,
                                              std::uint32_t index_,
                                              bool multi_) {
      auto glTFOutBaseName = fs::path(_outFile).stem();
      auto bufferOutPath =
          fs::path(_outFile).parent_path() /
          (multi_ ? (glTFOutBaseName.string() + std::to_string(index_) + ".bin")
                  : (glTFOutBaseName.string() + ".bin"));
      fs::create_directories(bufferOutPath.parent_path());

      std::ofstream ofs(bufferOutPath, std::ios::binary);
      ofs.exceptions(std::ios::badbit | std::ios::failbit);
      ofs.write(reinterpret_cast<const char *>(data_), size_);
      ofs.flush();

      auto outFileDir = fs::path(_outFile).parent_path();
      return relativeUriBetweenPath(outFileDir, bufferOutPath);
    }

  private:
    std::string_view _inFile;
    std::string_view _outFile;
    bool _dataUriForBuffers;
  };

  MyWriter writer{inputFile, outFile};
  convertOptions.useDataUriForBuffers = false;
  convertOptions.writer = &writer;

  const auto imageSearchDepth = 2;
  const std::array<const char *, 4> searchDirName = {"texture", "textures",
                                                     "material", "materials"};
  auto searchParentDir = fs::path(inputFile).parent_path();
  for (int i = 0; i < imageSearchDepth && !searchParentDir.empty();
       ++i, searchParentDir = searchParentDir.parent_path()) {
    convertOptions.textureSearch.locations.push_back(searchParentDir.string());
    for (auto &name : searchDirName) {
      convertOptions.textureSearch.locations.push_back(
          (searchParentDir / name).string());
    }
  }

  convertOptions.pathMode = bee::ConvertOptions::PathMode::copy;

  try {
    auto glTFJson = bee::convert(inputFile, convertOptions);

    std::ofstream glTFJsonOStream(outFile);
    glTFJsonOStream.exceptions(std::ios::badbit | std::ios::failbit);
    glTFJsonOStream << glTFJson;
    glTFJsonOStream.flush();
  } catch (const std::exception &exception) {
    std::cerr << exception.what() << "\n";
  }

  return 0;
}