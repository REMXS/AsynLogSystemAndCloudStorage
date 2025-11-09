#pragma once

#include "test_helper.h"
#include "Level.hpp"

TEST(LevelTest,base_test)
{
    using namespace asynclog;
    ASSERT_EQ(LogLevel::toString(LogLevel::value::DEBUG),"DEBUG");
    ASSERT_EQ(LogLevel::toString(LogLevel::value::INFO),"INFO");
    ASSERT_EQ(LogLevel::toString(LogLevel::value::WARN),"WARN");
    ASSERT_EQ(LogLevel::toString(LogLevel::value::ERROR),"ERROR");
    ASSERT_EQ(LogLevel::toString(LogLevel::value::FATAL),"FATAL");
}