#pragma once

#include <cassert>
#include <fbxsdk.h>
#include <vector>

namespace bee {
constexpr static auto defaultEplislon = static_cast<fbxsdk::FbxDouble>(1e-5);

inline bool
isApproximatelyEqual(fbxsdk::FbxDouble from_,
                     fbxsdk::FbxDouble to_,
                     fbxsdk::FbxDouble eplislon_ = defaultEplislon) {
  return std::abs(from_ - to_) < eplislon_;
}

template <int Size_>
inline bool
isApproximatelyEqual(const fbxsdk::FbxDouble *from_,
                     const fbxsdk::FbxDouble *to_,
                     fbxsdk::FbxDouble eplislon_ = defaultEplislon) {
  for (int i = 0; i < Size_; ++i) {
    if (!isApproximatelyEqual(from_[i], to_[i], eplislon_)) {
      return false;
    }
  }
  return true;
}

inline fbxsdk::FbxDouble
lerp(fbxsdk::FbxDouble from_, fbxsdk::FbxDouble to_, fbxsdk::FbxDouble rate_) {
  return from_ + (to_ - from_) * rate_;
}

inline fbxsdk::FbxVector4 lerp(const fbxsdk::FbxVector4 &from_,
                               const fbxsdk::FbxVector4 &to_,
                               fbxsdk::FbxDouble rate_) {
  return fbxsdk::FbxVector4(
      lerp(from_[0], to_[0], rate_), lerp(from_[1], to_[1], rate_),
      lerp(from_[2], to_[2], rate_), lerp(from_[3], to_[3], rate_));
}

inline fbxsdk::FbxQuaternion slerp(const fbxsdk::FbxQuaternion &from_,
                                   const fbxsdk::FbxQuaternion &to_,
                                   fbxsdk::FbxDouble rate_) {
  return from_.Slerp(to_, rate_);
}

template <typename Ty> struct TrackValueTrait {};

template <> struct TrackValueTrait<fbxsdk::FbxVector4> {
  static bool isEqualApproximately(const fbxsdk::FbxVector4 &a_,
                                   const fbxsdk::FbxVector4 &b_,
                                   double epsilon_) {
    return isApproximatelyEqual<3>(a_, b_, epsilon_);
  }

  static fbxsdk::FbxVector4 interpolate(const fbxsdk::FbxVector4 &a_,
                                        const fbxsdk::FbxVector4 &b_,
                                        double t_) {
    return lerp(a_, b_, t_);
  }
};

template <> struct TrackValueTrait<fbxsdk::FbxQuaternion> {
  static bool isEqualApproximately(const fbxsdk::FbxQuaternion &a_,
                                   const fbxsdk::FbxQuaternion &b_,
                                   double epsilon_) {
    return isApproximatelyEqual<4>(a_, b_, epsilon_);
  }

  static fbxsdk::FbxQuaternion interpolate(const fbxsdk::FbxQuaternion &a_,
                                           const fbxsdk::FbxQuaternion &b_,
                                           double t_) {
    return slerp(a_, b_, t_);
  }
};

template <typename Ty> class Track {
public:
  std::vector<double> times;
  std::vector<Ty> values;

  void reduceLinearKeys(double epsilon_) {
    std::vector<double> newTimes;
    std::vector<Ty> newValues;
    const auto &originalTimes = times;
    const auto &originalValues = values;
    const auto nOriginalKeyframes = times.size();
    const auto canBeReduce =
        [&newTimes, &newValues, &originalTimes, &originalValues, epsilon_,
         nOriginalKeyframes](
            std::vector<double>::size_type original_keyframe_index_) {
          // If we haven't have any keyframe.
          if (newTimes.empty()) {
            // If this keyframe is not the last keyframe, and it's same with the
            // next keyframe, omit it.
            if (original_keyframe_index_ != (nOriginalKeyframes - 1)) {
              if (TrackValueTrait<Ty>::isEqualApproximately(
                      originalValues[original_keyframe_index_],
                      originalValues[original_keyframe_index_ + 1], epsilon_)) {
                return true;
              }
            }
            return false;
          }

          const auto time = originalTimes[original_keyframe_index_];
          const auto &value = originalValues[original_keyframe_index_];

          // If this is the last keyframe.
          if (original_keyframe_index_ == (nOriginalKeyframes - 1)) {
            // If's same with previous keyframe, omit it.
            if (TrackValueTrait<Ty>::isEqualApproximately(newValues.back(),
                                                          value, epsilon_)) {
              return true;
            }
            // Otherwise we must preserve it.
            return false;
          }

          const auto previousTime = newTimes.back();
          const auto previousValue = newValues.back();
          const auto nextTime = originalTimes[original_keyframe_index_ + 1];
          const auto nextValue = originalValues[original_keyframe_index_ + 1];

          const auto lenT = nextTime - previousTime;
          // If the time [prev, cur, next] is almost zero.
          if (isApproximatelyEqual(lenT, 0.0, epsilon_)) {
            // We can omit cur if prev¡Öcur¡Önext
            if (TrackValueTrait<Ty>::isEqualApproximately(previousValue, value,
                                                          epsilon_) &&
                TrackValueTrait<Ty>::isEqualApproximately(value, nextValue,
                                                          epsilon_)) {
              return true;
            }
            // Otherwise, even it's strange, we have to preseve it however.
            return false;
          }

          const auto t = (time - previousTime) / lenT;
          // If cur¡Ölerp(prev, next, t), we can omit it.
          if (const auto deducedMiddle =
                  TrackValueTrait<Ty>::interpolate(previousValue, nextValue, t);
              TrackValueTrait<Ty>::isEqualApproximately(deducedMiddle, value,
                                                        epsilon_)) {
            return true;
          }

          return false;
        };

    for (decltype(times.size()) iKeyframe = 0; iKeyframe < originalTimes.size();
         ++iKeyframe) {
      const auto time = originalTimes[iKeyframe];
      const auto value = originalValues[iKeyframe];
      const auto reducible = canBeReduce(iKeyframe);
      if (!reducible) {
        newTimes.push_back(time);
        newValues.push_back(value);
      }
    }

    newTimes.swap(times);
    newValues.swap(values);
  }

  void add(double time_, const Ty &value_) {
    times.push_back(time_);
    values.push_back(value_);
  }
};
} // namespace bee