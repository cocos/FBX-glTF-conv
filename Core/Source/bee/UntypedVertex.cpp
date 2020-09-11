#include <bee/UntypedVertex.h>
#include <cassert>

namespace bee {
std::tuple<UntypedVertex, std::uint32_t> UntypedVertexVector::allocate() {
  if (_nLastPageVertices >= _nVerticesPerPage || _vertexPages.empty()) {
    _vertexPages.emplace_back(
        std::make_unique<std::byte[]>(_vertexSize * _nVerticesPerPage));
    _nLastPageVertices = 0;
  }
  return {_vertexPages.back().get() + _vertexSize * _nLastPageVertices++,
          _nextVertexIndex++};
}

void UntypedVertexVector::pop_back() {
  assert((!_vertexPages.empty()) && _nLastPageVertices);
  --_nLastPageVertices;
  if (_nLastPageVertices == 0) {
    _vertexPages.pop_back();
    _nLastPageVertices = _nVerticesPerPage;
  }
  --_nextVertexIndex;
}

std::unique_ptr<std::byte[]> UntypedVertexVector::merge() {
  auto data = std::make_unique<std::byte[]>(_vertexSize * size());
  if (!_vertexPages.empty()) {
    auto iPage = _vertexPages.begin();
    auto iLastFullPage = _vertexPages.end();
    --iLastFullPage;
    auto pData = data.get();
    const auto fullPageBytes = _vertexSize * _nVerticesPerPage;
    for (; iPage != iLastFullPage; ++iPage, pData += fullPageBytes) {
      std::memcpy(pData, (*iPage).get(), fullPageBytes);
    }
    std::memcpy(pData, (*iPage).get(), _vertexSize * _nLastPageVertices);
  }
  return data;
}

std::size_t UntypedVertexHasher::operator()(const UntypedVertex &vertex_) const  {
  // Referenced from the excellent fbx2glTF VertexHasher
  auto position = reinterpret_cast<const double *>(vertex_);
  std::size_t seed = 5381;
  const auto hasher = std::hash<
      std::remove_const_t<std::remove_pointer_t<decltype(position)>>>{};
  seed ^= hasher(position[0]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= hasher(position[1]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= hasher(position[2]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}
} // namespace bee
