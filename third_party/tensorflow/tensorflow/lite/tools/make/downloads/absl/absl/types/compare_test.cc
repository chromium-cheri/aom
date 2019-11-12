// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/types/compare.h"

#include "gtest/gtest.h"
#include "absl/base/casts.h"

namespace absl {
namespace {

// This is necessary to avoid a bunch of lint warnings suggesting that we use
// EXPECT_EQ/etc., which doesn't work in this case because they convert the `0`
// to an int, which can't be converted to the unspecified zero type.
bool Identity(bool b) { return b; }

TEST(Compare, WeakEquality) {
  EXPECT_TRUE(Identity(weak_equality::equivalent == 0));
  EXPECT_TRUE(Identity(0 == weak_equality::equivalent));
  EXPECT_TRUE(Identity(weak_equality::nonequivalent != 0));
  EXPECT_TRUE(Identity(0 != weak_equality::nonequivalent));
}

TEST(Compare, StrongEquality) {
  EXPECT_TRUE(Identity(strong_equality::equal == 0));
  EXPECT_TRUE(Identity(0 == strong_equality::equal));
  EXPECT_TRUE(Identity(strong_equality::nonequal != 0));
  EXPECT_TRUE(Identity(0 != strong_equality::nonequal));
  EXPECT_TRUE(Identity(strong_equality::equivalent == 0));
  EXPECT_TRUE(Identity(0 == strong_equality::equivalent));
  EXPECT_TRUE(Identity(strong_equality::nonequivalent != 0));
  EXPECT_TRUE(Identity(0 != strong_equality::nonequivalent));
}

TEST(Compare, PartialOrdering) {
  EXPECT_TRUE(Identity(partial_ordering::less < 0));
  EXPECT_TRUE(Identity(0 > partial_ordering::less));
  EXPECT_TRUE(Identity(partial_ordering::less <= 0));
  EXPECT_TRUE(Identity(0 >= partial_ordering::less));
  EXPECT_TRUE(Identity(partial_ordering::equivalent == 0));
  EXPECT_TRUE(Identity(0 == partial_ordering::equivalent));
  EXPECT_TRUE(Identity(partial_ordering::greater > 0));
  EXPECT_TRUE(Identity(0 < partial_ordering::greater));
  EXPECT_TRUE(Identity(partial_ordering::greater >= 0));
  EXPECT_TRUE(Identity(0 <= partial_ordering::greater));
  EXPECT_TRUE(Identity(partial_ordering::unordered != 0));
  EXPECT_TRUE(Identity(0 != partial_ordering::unordered));
  EXPECT_FALSE(Identity(partial_ordering::unordered < 0));
  EXPECT_FALSE(Identity(0 < partial_ordering::unordered));
  EXPECT_FALSE(Identity(partial_ordering::unordered <= 0));
  EXPECT_FALSE(Identity(0 <= partial_ordering::unordered));
  EXPECT_FALSE(Identity(partial_ordering::unordered > 0));
  EXPECT_FALSE(Identity(0 > partial_ordering::unordered));
  EXPECT_FALSE(Identity(partial_ordering::unordered >= 0));
  EXPECT_FALSE(Identity(0 >= partial_ordering::unordered));
}

TEST(Compare, WeakOrdering) {
  EXPECT_TRUE(Identity(weak_ordering::less < 0));
  EXPECT_TRUE(Identity(0 > weak_ordering::less));
  EXPECT_TRUE(Identity(weak_ordering::less <= 0));
  EXPECT_TRUE(Identity(0 >= weak_ordering::less));
  EXPECT_TRUE(Identity(weak_ordering::equivalent == 0));
  EXPECT_TRUE(Identity(0 == weak_ordering::equivalent));
  EXPECT_TRUE(Identity(weak_ordering::greater > 0));
  EXPECT_TRUE(Identity(0 < weak_ordering::greater));
  EXPECT_TRUE(Identity(weak_ordering::greater >= 0));
  EXPECT_TRUE(Identity(0 <= weak_ordering::greater));
}

TEST(Compare, StrongOrdering) {
  EXPECT_TRUE(Identity(strong_ordering::less < 0));
  EXPECT_TRUE(Identity(0 > strong_ordering::less));
  EXPECT_TRUE(Identity(strong_ordering::less <= 0));
  EXPECT_TRUE(Identity(0 >= strong_ordering::less));
  EXPECT_TRUE(Identity(strong_ordering::equal == 0));
  EXPECT_TRUE(Identity(0 == strong_ordering::equal));
  EXPECT_TRUE(Identity(strong_ordering::equivalent == 0));
  EXPECT_TRUE(Identity(0 == strong_ordering::equivalent));
  EXPECT_TRUE(Identity(strong_ordering::greater > 0));
  EXPECT_TRUE(Identity(0 < strong_ordering::greater));
  EXPECT_TRUE(Identity(strong_ordering::greater >= 0));
  EXPECT_TRUE(Identity(0 <= strong_ordering::greater));
}

TEST(Compare, Conversions) {
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(strong_equality::equal) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(strong_equality::nonequal) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(strong_equality::equivalent) == 0));
  EXPECT_TRUE(Identity(
      implicit_cast<weak_equality>(strong_equality::nonequivalent) != 0));

  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(partial_ordering::less) != 0));
  EXPECT_TRUE(Identity(
      implicit_cast<weak_equality>(partial_ordering::equivalent) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(partial_ordering::greater) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(partial_ordering::unordered) != 0));

  EXPECT_TRUE(implicit_cast<weak_equality>(weak_ordering::less) != 0);
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(weak_ordering::equivalent) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(weak_ordering::greater) != 0));

  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(weak_ordering::less) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(weak_ordering::less) < 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(weak_ordering::less) <= 0));
  EXPECT_TRUE(Identity(
      implicit_cast<partial_ordering>(weak_ordering::equivalent) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(weak_ordering::greater) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(weak_ordering::greater) > 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(weak_ordering::greater) >= 0));

  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(strong_ordering::less) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(strong_ordering::equal) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(strong_ordering::equivalent) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_equality>(strong_ordering::greater) != 0));

  EXPECT_TRUE(
      Identity(implicit_cast<strong_equality>(strong_ordering::less) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<strong_equality>(strong_ordering::equal) == 0));
  EXPECT_TRUE(Identity(
      implicit_cast<strong_equality>(strong_ordering::equivalent) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<strong_equality>(strong_ordering::greater) != 0));

  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(strong_ordering::less) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(strong_ordering::less) < 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(strong_ordering::less) <= 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(strong_ordering::equal) == 0));
  EXPECT_TRUE(Identity(
      implicit_cast<partial_ordering>(strong_ordering::equivalent) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(strong_ordering::greater) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(strong_ordering::greater) > 0));
  EXPECT_TRUE(
      Identity(implicit_cast<partial_ordering>(strong_ordering::greater) >= 0));

  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::less) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::less) < 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::less) <= 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::equal) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::equivalent) == 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::greater) != 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::greater) > 0));
  EXPECT_TRUE(
      Identity(implicit_cast<weak_ordering>(strong_ordering::greater) >= 0));
}

struct WeakOrderingLess {
  template <typename T>
  absl::weak_ordering operator()(const T &a, const T &b) const {
    return a < b ? absl::weak_ordering::less
                 : a == b ? absl::weak_ordering::equivalent
                          : absl::weak_ordering::greater;
  }
};

TEST(CompareResultAsLessThan, SanityTest) {
  EXPECT_FALSE(absl::compare_internal::compare_result_as_less_than(false));
  EXPECT_TRUE(absl::compare_internal::compare_result_as_less_than(true));

  EXPECT_TRUE(
      absl::compare_internal::compare_result_as_less_than(weak_ordering::less));
  EXPECT_FALSE(absl::compare_internal::compare_result_as_less_than(
      weak_ordering::equivalent));
  EXPECT_FALSE(absl::compare_internal::compare_result_as_less_than(
      weak_ordering::greater));
}

TEST(DoLessThanComparison, SanityTest) {
  std::less<int> less;
  WeakOrderingLess weak;

  EXPECT_TRUE(absl::compare_internal::do_less_than_comparison(less, -1, 0));
  EXPECT_TRUE(absl::compare_internal::do_less_than_comparison(weak, -1, 0));

  EXPECT_FALSE(absl::compare_internal::do_less_than_comparison(less, 10, 10));
  EXPECT_FALSE(absl::compare_internal::do_less_than_comparison(weak, 10, 10));

  EXPECT_FALSE(absl::compare_internal::do_less_than_comparison(less, 10, 5));
  EXPECT_FALSE(absl::compare_internal::do_less_than_comparison(weak, 10, 5));
}

TEST(CompareResultAsOrdering, SanityTest) {
  EXPECT_TRUE(Identity(
      absl::compare_internal::compare_result_as_ordering(-1) < 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::compare_result_as_ordering(-1) == 0));
  EXPECT_FALSE(
      Identity(absl::compare_internal::compare_result_as_ordering(-1) > 0));
  EXPECT_TRUE(Identity(absl::compare_internal::compare_result_as_ordering(
                           weak_ordering::less) < 0));
  EXPECT_FALSE(Identity(absl::compare_internal::compare_result_as_ordering(
                            weak_ordering::less) == 0));
  EXPECT_FALSE(Identity(absl::compare_internal::compare_result_as_ordering(
                            weak_ordering::less) > 0));

  EXPECT_FALSE(Identity(
      absl::compare_internal::compare_result_as_ordering(0) < 0));
  EXPECT_TRUE(Identity(
      absl::compare_internal::compare_result_as_ordering(0) == 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::compare_result_as_ordering(0) > 0));
  EXPECT_FALSE(Identity(absl::compare_internal::compare_result_as_ordering(
                           weak_ordering::equivalent) < 0));
  EXPECT_TRUE(Identity(absl::compare_internal::compare_result_as_ordering(
                            weak_ordering::equivalent) == 0));
  EXPECT_FALSE(Identity(absl::compare_internal::compare_result_as_ordering(
                            weak_ordering::equivalent) > 0));

  EXPECT_FALSE(Identity(
      absl::compare_internal::compare_result_as_ordering(1) < 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::compare_result_as_ordering(1) == 0));
  EXPECT_TRUE(Identity(
      absl::compare_internal::compare_result_as_ordering(1) > 0));
  EXPECT_FALSE(Identity(absl::compare_internal::compare_result_as_ordering(
                           weak_ordering::greater) < 0));
  EXPECT_FALSE(Identity(absl::compare_internal::compare_result_as_ordering(
                            weak_ordering::greater) == 0));
  EXPECT_TRUE(Identity(absl::compare_internal::compare_result_as_ordering(
                            weak_ordering::greater) > 0));
}

TEST(DoThreeWayComparison, SanityTest) {
  std::less<int> less;
  WeakOrderingLess weak;

  EXPECT_TRUE(Identity(
      absl::compare_internal::do_three_way_comparison(less, -1, 0) < 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(less, -1, 0) == 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(less, -1, 0) > 0));
  EXPECT_TRUE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, -1, 0) < 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, -1, 0) == 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, -1, 0) > 0));

  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(less, 10, 10) < 0));
  EXPECT_TRUE(Identity(
      absl::compare_internal::do_three_way_comparison(less, 10, 10) == 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(less, 10, 10) > 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, 10, 10) < 0));
  EXPECT_TRUE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, 10, 10) == 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, 10, 10) > 0));

  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(less, 10, 5) < 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(less, 10, 5) == 0));
  EXPECT_TRUE(Identity(
      absl::compare_internal::do_three_way_comparison(less, 10, 5) > 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, 10, 5) < 0));
  EXPECT_FALSE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, 10, 5) == 0));
  EXPECT_TRUE(Identity(
      absl::compare_internal::do_three_way_comparison(weak, 10, 5) > 0));
}

}  // namespace
}  // namespace absl
