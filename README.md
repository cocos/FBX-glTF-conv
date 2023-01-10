
# FBX-glTF-conv

This is a FBX to glTF file format converter.

# Prerequisites

This project has been tested on:

- Windows

- macOS 10.13 or later

- Ubuntu 22.04

  > Ubuntu 20.04 is not supported.


# How to use

You can find the latest release in [releases page](https://github.com/cocos-creator/FBX-glTF-conv/releases).

To convert, run the executable at command line:

```ps1
> FBX-glTF-conv <source-FBX-file> --out <glTF-files-out-dir>
```

There are some options, run the executable without any arguments:

```ps1
> FBX-glTF-conv

This is a FBX to glTF file format converter.
Usage:
  FBX-glTF-conv [OPTION...] positional parameters

      --fbm-dir arg             The directory to store the embedded media.
      --out arg                 The output path to the .gltf or .glb file.
                                Defaults to
                                `<working-directory>/<FBX-filename-basename>
                                .gltf`
      --no-flip-v               Do not flip V texture coordinates.
      --unit-conversion arg     How to perform unit converseion.
                                  - `geometry-level` Do unit conversion at
                                geometry level.
                                  - `hierarchy-level` Do unit conversion at
                                hierarchy level.
                                  - `disabled` Disable unit conversion.
                                This may cause the generated glTF does't
                                conform to glTF specification. (default:
                                geometry-level)
      --no-texture-resolution   Do not resolve textures.
      --prefer-local-time-span  Prefer local time spans recorded in FBX
                                file for animation exporting. (default:
                                true)
      --animation-bake-rate arg
                                Animation bake rate(in FPS). (default: 30)
      --texture-search-locations arg
                                Texture search locations. These path shall
                                be absolute path or relative path from
                                input file's directory.
      --export-fbx-file-header-info
                                Export FBX file header info.
      --export-raw-materials    Export raw materials.
      --verbose                 Verbose output.
      --log-file arg            Specify the log file(logs are outputed as
                                JSON). If not specified, logs're printed to
                                console
      --image-path-mode arg     Specify the mode used to specify the image
                                path. Could be one of the following:
                                - absolute - Output the absolute path to
                                the path.
                                - relative - Output the relative path to
                                the path.
                                - prefer-relative - If the image is under
                                the the same or sub directory of glTF file,
                                output as relative. Otherwise output as
                                absolute.
                                - strip - Ingore all path information, only
                                output the file name.
                                - embedded - Embedded the image into Data
                                URI.
                                - copy - Copy the image to the output
                                directory and reference it with a relative
                                path.
```

## Build

To build this tool, the followings are required:

- Windows or MacOS;

- [FBX SDK 2019 or higher](https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2020-0);

- [vcpkg](https://github.com/microsoft/vcpkg)

- CMake

This is a CMake project, just build it in normal CMake build process. However you need to indicate the FBXSDK's location and vcpkg toolchain file:

```ps1
> cmake -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake" -DFbxSdkHome:STRING="<path-to-FBX-SDK-home>"
```

If problems encountered, you may file an issue or reference to the [CI build script](./CI/GitHubBuild.ps1).

## Why

This tool is essentially used as a part of the Cocos Creator.
In former, Cocos Creator supports FBX file format through the excellent [FBX2glTF](https://github.com/facebookincubator/FBX2glTF).

But Cocos team has to find another approach because:

* FBX2glTF store the glTF result files onto disk and Creator read the files.
  This is the only way that Creator can communicate with FBX2glTF. File system I/O is slow.
* Author of FBX2glTF is tired.
* FBX is complex and all exporters working for it are buggy. We usually need to fix strange issues. It's hard to sync fixes between Cocos and Facebook.

## Features

* 🗸 Geometries

  * 🗸 Meshes

* 🗸 Materials

  * 🗸 Lambert and Phong

* 🗸 Textures and images

  * 🗸 Image formats: JPEG, PNG

* 🗸 Skinning

  * ⌛ Cluster mode: additive

* 🗸 Blend shapes(Morph targets)

* 🗸 Animations

  * 🗸 Node transform animations(Skeletal animations)

  * 🗸 Blend shape animations(Morph animations)

* 🗸 Scene hierarchy

  * ⌛ FBX specific node inherit types: `RrSs`, `Rrs`

* ⌛ Cameras

* ⌛ Lights

🗸 Supported ⌛ Not finished

## Thanks

Again, the FBX is complex and specification-less. In development, we often reference from or are inspired from the following predecessors:

* [FBX2glTF](https://github.com/facebookincubator/FBX2glTF)

* [claygl](https://github.com/pissang/claygl)
