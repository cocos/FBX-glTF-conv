#include "bee/Convert/AnimationUtility.h"
#include <cmath>
#include <doctest/doctest.h>
#include <string>

namespace bee {
// Note: we have already had specificalization for fbx double!
static_assert(std::is_same_v<fbxsdk::FbxDouble, double>);
} // namespace bee

using DoubleKeyframe = std::pair<double, double>;

template <template <typename Ty> class Vec = std::initializer_list>
auto reduceLinearKeys(const Vec<DoubleKeyframe> &keys_,
                      double epsilon_ = bee::defaultEplislon) {
  bee::Track<double> track;
  for (const auto [k, v] : keys_) {
    track.add(k, v);
  }
  track.reduceLinearKeys(epsilon_);
  std::vector<DoubleKeyframe> result;
  for (decltype(track.times.size()) iKey = 0; iKey < track.times.size();
       ++iKey) {
    result.push_back({track.times[iKey], track.values[iKey]});
  }
  return result;
}

template <class F, class... Args> void for_each_argument(F f, Args &&...args) {
  [](...) {}((f(std::forward<Args>(args)), 0)...);
}

template <typename... Vecs> auto concatVecs(Vecs... vecs) {
  std::vector<DoubleKeyframe> result;
  ( // https://stackoverflow.com/a/60136761
      [&] {
        for (const auto &kv : vecs) {
          result.push_back(kv);
        }
      }(),
      ...);
  return result;
}

TEST_CASE("Linear Key Reduction/0 keys") {
  {
    const auto r = reduceLinearKeys({});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{});
  }
}

TEST_CASE("Linear Key Reduction/1 keys") {
  {
    const auto r = reduceLinearKeys({{0.2, 0.4}});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{{0.2, 0.4}});
  }
}

TEST_CASE("Linear Key Reduction/2 keys") {
  // Times are too close, values are preserved.
  {
    const auto r = reduceLinearKeys(
        {{0.15, 0.4}, {0.15 + bee::defaultEplislon * 1e-1, 0.1}});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{
                    {0.15, 0.4}, {0.15 + bee::defaultEplislon * 1e-1, 0.1}});
  }

  // Values are not same.
  {
    const auto r = reduceLinearKeys({{0.15, 0.4}, {0.7, 0.1}});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{{0.15, 0.4}, {0.7, 0.1}});
  }

  // Values are too close.
  {
    const auto r = reduceLinearKeys(
        {{0.15, 0.4}, {0.7, 0.4 + (bee::defaultEplislon * 1e-1)}});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{
                    {0.7, 0.4 + (bee::defaultEplislon * 1e-1)}});
  }
}

TEST_CASE("Linear Key Reduction/3 keys") {
  // Previous 2 times are too close, values are preserved.
  {
    const auto r = reduceLinearKeys(
        {{0.15, 0.4}, {0.15 + bee::defaultEplislon * 1e-1, 0.1}, {0.16, 0.9}});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{
                    {0.15, 0.4},
                    {0.15 + bee::defaultEplislon * 1e-1, 0.1},
                    {0.16, 0.9}});
  }

  // Last 2 times are too close, values are preserved.
  {
    const auto r = reduceLinearKeys(
        {{0.15, 0.4}, {0.16, 0.1}, {0.16 + bee::defaultEplislon * 1e-1, 0.9}});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{
                    {0.15, 0.4},
                    {0.16, 0.1},
                    {0.16 + bee::defaultEplislon * 1e-1, 0.9}});
  }

  // All 3 times are too close, values are preserved.
  {
    const auto r =
        reduceLinearKeys({{0.15, 0.4},
                          {0.15 + bee::defaultEplislon * 1e-1, 0.1},
                          {0.15 + bee::defaultEplislon * 1e-1 * 2.0, 0.9}});
    CHECK_EQ(r, std::vector<DoubleKeyframe>{
                    {0.15, 0.4},
                    {0.15 + bee::defaultEplislon * 1e-1, 0.1},
                    {0.15 + bee::defaultEplislon * 1e-1 * 2.0, 0.9}});
  }
}

TEST_CASE("Linear Key Reduction/Insert constant keys between multiple "
          "successive linear keys") {
  const auto r = reduceLinearKeys({{0.2, 0.7},
                                   {0.25, 0.6},
                                   {0.27, 0.6},
                                   {0.28, 0.6},
                                   {0.3, 0.5},
                                   {0.35, 0.4}});
  CHECK_EQ(r, std::vector<DoubleKeyframe>{
                  {0.2, 0.7},
                  {0.25, 0.6},
                  {0.28, 0.6},
                  {0.3, 0.5},
                  {0.35, 0.4},
              });
}

TEST_CASE("Linear Key Reduction/Successive linear keys") {

  const auto prevKeys = std::vector<DoubleKeyframe>{{0.1, 0.3}};
  const auto successiveObservee = std::vector<DoubleKeyframe>{
      {0.2, 0.7}, {0.25, 0.6}, {0.3, 0.5}, {0.35, 0.4}};
  const auto nextKeys = std::vector<DoubleKeyframe>{{0.8, -0.2}};

  SUBCASE("At begin") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(successiveObservee, nextKeys));
    CHECK_EQ(r,
             concatVecs(std::vector<DoubleKeyframe>{successiveObservee.front(),
                                                    successiveObservee.back()},
                        nextKeys));
  }

  SUBCASE("At end") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(prevKeys, successiveObservee));
    CHECK_EQ(r, concatVecs(prevKeys, std::vector<DoubleKeyframe>{
                                         successiveObservee.front(),
                                         successiveObservee.back()}));
  }

  SUBCASE("At middle") {
    const auto r = reduceLinearKeys<std::vector>(
        concatVecs(prevKeys, successiveObservee, nextKeys));
    CHECK_EQ(r,
             concatVecs(prevKeys,
                        std::vector<DoubleKeyframe>{successiveObservee.front(),
                                                    successiveObservee.back()},
                        nextKeys));
  }

  SUBCASE("All keyframes are the same") {
    const auto r = reduceLinearKeys<std::vector>(successiveObservee);
    CHECK_EQ(r, std::vector<DoubleKeyframe>{successiveObservee.front(),
                                            successiveObservee.back()});
  }
}

TEST_CASE("Linear Key Reduction/Successive same keys") {

  const auto prevKeys = std::vector<DoubleKeyframe>{{0.16, 0.4}};
  const auto successiveObservee =
      std::vector<DoubleKeyframe>{{0.2, 0.7},
                                  {0.21, 0.7 + bee::defaultEplislon * 1e-1},
                                  {0.3, 0.7 - bee::defaultEplislon * 1e-1}};
  const auto nextKeys = std::vector<DoubleKeyframe>{{0.4, 0.8}};

  SUBCASE("At begin") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(successiveObservee, nextKeys));
    CHECK_EQ(r,
             concatVecs(std::vector<DoubleKeyframe>{successiveObservee.back()},
                        nextKeys));
  }

  SUBCASE("At end") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(prevKeys, successiveObservee));
    CHECK_EQ(r, concatVecs(prevKeys, std::vector<DoubleKeyframe>{
                                         successiveObservee.front()}));
  }

  SUBCASE("At middle") {
    const auto r = reduceLinearKeys<std::vector>(
        concatVecs(prevKeys, successiveObservee, nextKeys));
    CHECK_EQ(r,
             concatVecs(prevKeys,
                        std::vector<DoubleKeyframe>{successiveObservee.front(),
                                                    successiveObservee.back()},
                        nextKeys));
  }

  SUBCASE("All keyframes are the same") {
    const auto r = reduceLinearKeys<std::vector>(successiveObservee);
    CHECK_EQ(r, std::vector<DoubleKeyframe>{successiveObservee.back()});
  }
}

TEST_CASE("Linear Key Reduction/Successive same times but different values") {
  const auto prevKeys = std::vector<DoubleKeyframe>{{0.16, 0.4}};
  const auto successiveObservee = std::vector<DoubleKeyframe>{
      {0.2, 0.7},
      {0.2, 0.8},
      {0.2 + bee::defaultEplislon * 1e-1, 0.9},
  };
  const auto nextKeys = std::vector<DoubleKeyframe>{{0.4, 0.8}};

  // All cases should have no reduction happened.

  SUBCASE("At begin") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(successiveObservee, nextKeys));
    CHECK_EQ(r, concatVecs(successiveObservee, nextKeys));
  }

  SUBCASE("At end") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(prevKeys, successiveObservee));
    CHECK_EQ(r, concatVecs(prevKeys, successiveObservee));
  }

  SUBCASE("At middle") {
    const auto r = reduceLinearKeys<std::vector>(
        concatVecs(prevKeys, successiveObservee, nextKeys));
    CHECK_EQ(r, concatVecs(prevKeys, successiveObservee, nextKeys));
  }

  SUBCASE("All keyframes are the same") {
    const auto r = reduceLinearKeys<std::vector>(successiveObservee);
    CHECK_EQ(r, successiveObservee);
  }
}

TEST_CASE("Linear Key Reduction/Successive same times and same values") {
  const auto prevKeys = std::vector<DoubleKeyframe>{{0.16, 0.4}};
  const auto successiveObservee = std::vector<DoubleKeyframe>{
      {0.3, 0.7},
      {0.3, 0.7},
      {0.3 + bee::defaultEplislon * 1e-1, 0.7 + bee::defaultEplislon * 1e-1},
  };
  const auto nextKeys = std::vector<DoubleKeyframe>{{0.4, 0.8}};

  SUBCASE("At begin") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(successiveObservee, nextKeys));
    CHECK_EQ(r,
             concatVecs(std::vector<DoubleKeyframe>{successiveObservee.back()},
                        nextKeys));
  }

  SUBCASE("At end") {
    const auto r =
        reduceLinearKeys<std::vector>(concatVecs(prevKeys, successiveObservee));
    CHECK_EQ(r, concatVecs(prevKeys, std::vector<DoubleKeyframe>{
                                         successiveObservee.back()}));
  }

  SUBCASE("At middle") {
    const auto r = reduceLinearKeys<std::vector>(
        concatVecs(prevKeys, successiveObservee, nextKeys));
    CHECK_EQ(r,
             concatVecs(prevKeys,
                        std::vector<DoubleKeyframe>{successiveObservee.back()},
                        nextKeys));
  }

  SUBCASE("All keyframes are the same") {
    const auto r = reduceLinearKeys<std::vector>(successiveObservee);
    CHECK_EQ(r, std::vector<DoubleKeyframe>{successiveObservee.back()});
  }
}

TEST_CASE("Linear Key Reduction") {
  {
    const auto reduce = [](double epislon_ = bee::defaultEplislon) {
      return reduceLinearKeys(
          // Keys are adapted from
          // https://nfrechette.github.io/2016/12/07/anim_compression_key_reduction/
          // .
          {
              {1.0, 0.6},
              {2.0, 0.35},
              {3.0, 0.6},
              {4.0, 0.85},
              {5.0, 0.96},
              {6.0, 0.8},
              {7.0, 0.2},
              {8.0, 0.7},
          },
          epislon_);
    };

    {
      const auto r = reduce();
      CHECK_EQ(r, std::vector<DoubleKeyframe>{
                      {1.0, 0.6},
                      {2.0, 0.35},
                      {4.0, 0.85},
                      {5.0, 0.96},
                      {6.0, 0.8},
                      {7.0, 0.2},
                      {8.0, 0.7},
                  });
    }

    {
      const auto r = reduce(1e-2);
      CHECK_EQ(r, std::vector<DoubleKeyframe>{
                      {1.0, 0.6},
                      {2.0, 0.35},
                      {4.0, 0.85},
                      {5.0, 0.96},
                      {6.0, 0.8},
                      {7.0, 0.2},
                      {8.0, 0.7},
                  });
    }

    {
      const auto r = reduce(1e-1);
      CHECK_EQ(r, std::vector<DoubleKeyframe>{
                      {1.0, 0.6},
                      {2.0, 0.35},
                      {5.0, 0.96},
                      {6.0, 0.8},
                      {7.0, 0.2},
                      {8.0, 0.7},
                  });
    }
  }
}