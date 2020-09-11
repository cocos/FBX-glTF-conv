
#pragma once

#include <fbxsdk.h>

namespace bee {
class FbxObjectDestroyer {
private:
  FbxObject *_object;

public:
  FbxObjectDestroyer(FbxObject *object_) : _object(object_) {
  }
  FbxObjectDestroyer(FbxObjectDestroyer &&) = delete;
  FbxObjectDestroyer(const FbxObjectDestroyer &) = delete;
  ~FbxObjectDestroyer() {
    if (_object) {
      _object->Destroy();
      _object = nullptr;
    }
  }
};
} // namespace bee