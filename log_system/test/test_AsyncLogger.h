#pragma once
#include "test_helper.h"
#include <streambuf>

#include "AsyncLogger.hpp"


using namespace asynclog;

class MockSystemStrOps:public ISystemStrOps
{
public:
    MOCK_METHOD(int,vasprintf,(char **,const char *,va_list),(override));
};

class AsyncLoggerTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        pool=std::make_shared<ThreadPool>(2,1000);
    }
    void SetUp()override
    {
        json_data_.buffer_size_=1;
        std::vector<std::shared_ptr<LogFlush>>flush_;
        flush_.emplace_back(LogFlushFactory<StdOutFlush>::createLogFlush());
        
        test_logger=std::make_unique<AsyncLogger>("test_log",flush_,pool,json_data_);
    }
    void TearDown()
    {

    }
    Util::JsonUtil::JsonData json_data_;
    std::unique_ptr<AsyncLogger>test_logger;
    inline static std::shared_ptr<ThreadPool>pool;
};

TEST_F(AsyncLoggerTest,single_message_test)
{
    std::streambuf* old_cout_buf = std::cout.rdbuf();
    std::ostringstream captured_cout;
    std::cout.rdbuf(captured_cout.rdbuf());

    test_logger->info("test.cpp",10,"This is a test log message: %d",42);
    //等待一段时间以确保日志被异步处理
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    std::string output = captured_cout.str();
    //检查输出是否包含预期的日志内容
    EXPECT_THAT(output, ::testing::HasSubstr("[INFO][test_log][test.cpp:10]\tThis is a test log message: 42\n"));

    std::cout.rdbuf(old_cout_buf);
}

TEST_F(AsyncLoggerTest,vasprintf_fail_test)
{
    auto mock_ops=std::make_unique<MockSystemStrOps>();
    EXPECT_CALL(*mock_ops,vasprintf(testing::_,testing::_,testing::_))
        .WillOnce(testing::Return(-1));
        
    //重新构建含有mock的test_logger
    test_logger=std::make_unique<AsyncLogger>("test_log",
        std::vector<std::shared_ptr<LogFlush>>{
            LogFlushFactory<StdOutFlush>::createLogFlush()
        },pool,json_data_,BufferPolicy::UNLIMITED,16*1024,std::move(mock_ops));

    bool ret=test_logger->debug("test.cpp",20,"This message will fail vasprintf: %s","failure");
    EXPECT_FALSE(ret);
}

TEST_F(AsyncLoggerTest,async_logger_builder_test)
{
    AsyncLoggerBuilder builder;
    builder.setLoggerName("builder_logger");
    builder.setBufferPolicy(BufferPolicy::LIMIT_SIZE);
    builder.setMaxBufferSize(32*1024); //32KB

    Util::JsonUtil::JsonData config_data;
    config_data.flush_log_=2; //使用fsync
    builder.setConfig(config_data);

    builder.addLogFlush<StdOutFlush>();

    auto logger=builder.build(pool);
    EXPECT_EQ(logger->info("builder.cpp",30,"Logger built by AsyncLoggerBuilder."),true);
}




