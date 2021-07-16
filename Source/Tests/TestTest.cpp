#include "BaseTest.h"

#include <gtest/gtest.h>

TEST_F(BaseTest, FirstTest)
{
    int a = 10;
    ASSERT_EQ(a, 10);
}
