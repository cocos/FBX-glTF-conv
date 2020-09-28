
#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace bee {
template <typename FinalError_> class ErrorBase {};

template <typename FinalError_>
void to_json(nlohmann::json &j_, const ErrorBase<FinalError_> &error_) {
  j_ = {{"code", std::string_view{
                     reinterpret_cast<const char *>(FinalError_::code.data()),
                     FinalError_::code.size()}}};
}

template <typename FinalError_>
class NodeError : public ErrorBase<FinalError_> {
public:
  NodeError(std::string_view node_) : _node(node_) {
  }

  std::string_view node() const {
    return _node;
  }

private:
  std::string _node;
};

template <typename FinalError_>
void to_json(nlohmann::json &j_, const NodeError<FinalError_> &error_) {
  to_json(j_, static_cast<const ErrorBase<FinalError_> &>(error_));
  j_["node"] = error_.node();
}

template <typename FinalError_>
class MeshError : public NodeError<FinalError_> {
public:
  using NodeError<FinalError_>::NodeError;
};

template <typename FinalError_>
void to_json(nlohmann::json &j_, const MeshError<FinalError_> &error_) {
  to_json(j_, static_cast<const NodeError<FinalError_> &>(error_));
  j_["mesh"] = error_.node();
}
} // namespace bee