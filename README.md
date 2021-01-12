
# FBX-glTF-conv

This is a FBX to glTF file format converter.

## Why

This tool is essentially used as a part of the Cocos Creator.
In former, Cocos Creator supports FBX file format through the excellent [FBX2glTF](https://github.com/facebookincubator/FBX2glTF).

But Cocos team has to find another approach because:

* FBX2glTF store the glTF result files onto disk and Creator read the files.
  This is the only way that Creator can communicate with FBX2glTF. File system I/O is slow.
* Author of FBX2glTF is tired.
* FBX is complex and all exporters working for it are buggy. We usually need to fix strange issues. It's hard to sync fixes between Cocos and Facebook.

## Features

- 🗸 Geometries

  - 🗸 Meshes

- 🗸 Materials

  - 🗸 Lambert and Phong

- 🗸 Textures and images

  - 🗸 Image formats: JPEG, PNG

- 🗸 Skinning

  - ⌛ Cluster mode: additive

- 🗸 Blend shapes(Morph targets)

- 🗸 Animations

  - 🗸 Node transform animations(Skeletal animations)

  - 🗸 Blend shape animations(Morph animations)

- 🗸 Scene hierarchy

  - ⌛ FBX specific node inherit types: `RrSs`, `Rrs`

- ⌛ Cameras

- ⌛ Lights

🗸 Supported ⌛ Not finished

## Thanks

Again, the FBX is complex and specification-less. In development, we often reference from or are inspired from the following predecessors:

- [FBX2glTF](https://github.com/facebookincubator/FBX2glTF)
- [claygl](https://github.com/pissang/claygl)
