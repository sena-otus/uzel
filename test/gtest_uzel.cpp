/**
 * @file gtest_uzel
 *
 * @brief test uzel
 *  */

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace {
using std::string;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

TEST(uzel, input1) {
  {
    EXPECT_EQ(1,1);
  }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
} // namespace
