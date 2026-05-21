#include <lithic3d/utils.hpp>
#include <gtest/gtest.h>

using namespace lithic3d;

class UtilsTest : public testing::Test
{
  public:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(UtilsTest, trimWhitespace)
{
  EXPECT_EQ("", trimWhitespace(""));
  EXPECT_EQ("A", trimWhitespace("A"));
  EXPECT_EQ("", trimWhitespace(" "));
  EXPECT_EQ("", trimWhitespace(" \n  \t \t\t \n "));
  EXPECT_EQ("Hello", trimWhitespace("Hello"));
  EXPECT_EQ("Hello", trimWhitespace("\n \t\t  Hello"));
  EXPECT_EQ("Hello", trimWhitespace("Hello\n  \n \t\t"));
  EXPECT_EQ("Hello", trimWhitespace(" \n \t\t  \tHello\n  \n \t\t "));
  EXPECT_EQ("Hello \t\n  \t World", trimWhitespace("  \t\n Hello \t\n  \t World \t \n"));
}
