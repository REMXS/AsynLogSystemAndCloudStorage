#include "test_helper.h"
#include "Message.hpp"

TEST(MessageTest,construct_test)
{
    using namespace asynclog;
    const LogLevel::value level = LogLevel::value::INFO;
    const std::string file = "test.cpp";
    const size_t line = 123;
    const std::string name = "TestLogger";
    const std::string payload = "Hello, world!";

    //创建 LogMessage 对象
    LogMessage msg(level,line,file,name,payload);

    //验证每个成员变量
    ASSERT_EQ(msg.level_, level);
    ASSERT_EQ(msg.file_name_, file);
    ASSERT_EQ(msg.line_, line);
    ASSERT_EQ(msg.name_, name);
    ASSERT_EQ(msg.pay_load_, payload);

    ASSERT_NEAR(msg.ctime_,Util::Date::now(),2);
    ASSERT_EQ(msg.tid_,std::this_thread::get_id());
}

TEST(MessageTest,format_test)
{
    using namespace asynclog;
    LogMessage msg;
    msg.level_ = LogLevel::value::DEBUG;
    msg.file_name_ = "main.cpp";
    msg.line_ = 88;
    msg.name_ = "WebServer";
    msg.pay_load_ = "Request received";

    std::stringstream ss;
    ss<<std::this_thread::get_id();
    msg.tid_=std::this_thread::get_id();

    struct tm t;
    t.tm_year = 2025-1900;
    t.tm_mon = 3-1;
    t.tm_mday = 3;
    t.tm_hour = 14;
    t.tm_min = 30;
    t.tm_sec = 0;
    t.tm_isdst = -1;

    time_t ti= mktime(&t);
    ASSERT_NE(ti,-1);
    msg.ctime_ = ti;

    std::string expected_string = "[2025-03-03 14:30:00][" + ss.str() + "][DEBUG][WebServer][main.cpp:88]\tRequest received\n";

    //验证日志格式是否正确
    ASSERT_EQ(expected_string,msg.format());
}
