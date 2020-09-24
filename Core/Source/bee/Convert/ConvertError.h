
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bee {
class ErrorBase {
protected:
};

class NodeError : public ErrorBase {
public:
  NodeError(std::string_view node_) : _node(node_) {
  }

  std::string_view node() const {
    return _node;
  }

private:
  std::string _node;
};

class MeshError : public NodeError {
public:
  using NodeError::NodeError;
};
} // namespace bee