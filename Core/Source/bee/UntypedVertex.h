
#pragma once

#include <cstddef>
#include <cstring>
#include <list>
#include <memory>
#include <vector>

namespace bee {
using UntypedVertex = std::byte *;

class UntypedVertexVector {
public:
  UntypedVertexVector(std::uint32_t vertex_size_) : _vertexSize(vertex_size_) {
  }

  std::uint32_t size() const {
    return _nextVertexIndex;
  }

  std::tuple<UntypedVertex, std::uint32_t> allocate();

  void pop_back();

  std::unique_ptr<std::byte[]> merge();

private:
  using VertexPage = std::unique_ptr<std::byte[]>;
  std::uint32_t _vertexSize;
  std::list<VertexPage> _vertexPages;
  std::uint32_t _nVerticesPerPage = 1024;
  std::uint32_t _nLastPageVertices = 0;
  std::uint32_t _nextVertexIndex = 0;
};

class UntypedVertexHasher {
public:
  std::size_t operator()(const UntypedVertex &vertex_) const;
};

class UntypedVertexEqual {
public:
  UntypedVertexEqual(std::size_t vertex_size_) : _vertexSize(vertex_size_) {
  }

  bool operator()(const UntypedVertex &lhs_, const UntypedVertex &rhs_) const {
    return 0 == std::memcmp(reinterpret_cast<const void *>(lhs_),
                            reinterpret_cast<const void *>(rhs_), _vertexSize);
  }

private:
  std::size_t _vertexSize;
};
} // namespace bee