#pragma once

#include "test_helper.h"

#include "LogFlush.hpp"

namespace fs = std::filesystem;
using namespace asynclog;
using ::testing::_;
using ::testing::Return;
using ::testing::InSequence; //验证调用顺序

class MockSystemOps: public ISystemOps
{
public:
    MOCK_METHOD(FILE*,fopen,(const char*, const char*),(override));
    MOCK_METHOD(int, fclose, (FILE*), (override));
    MOCK_METHOD(size_t, fwrite, (const void*, size_t, size_t, FILE*), (override));
    MOCK_METHOD(int, fflush, (FILE*), (override));
    MOCK_METHOD(int, fileno, (FILE*), (override));
    MOCK_METHOD(int, fsync, (int), (override));
    MOCK_METHOD(int, ferror, (FILE*), (override));
    MOCK_METHOD(void, perror, (const char*), (override));
    MOCK_METHOD(void, createDirectory, (const std::string&), (override));
    MOCK_METHOD(time_t, now, (), (override));
};


class LogFlushTest: public ::testing::Test
{
protected:
static void SetUpTestSuite()
{
}

static void TearDownTestSuite()
{
}

void SetUp()override
{
    json_data.flush_log_=1;
}


Util::JsonUtil::JsonData json_data;

//用于测试的虚假的FILE*变量
FILE* kFakeFile = (FILE*)0x12345678;

};


TEST_F(LogFlushTest,StdOutFlush_test)
{
    auto std_out_log=LogFlushFactory<StdOutFlush>::createLogFlush();
    std::streambuf* old_buf = std::cout.rdbuf();
    std::ostringstream oss;
    //重定向cout的缓冲区到oss中
    std::cout.rdbuf(oss.rdbuf());

    std::string data="hello world";

    std_out_log->flush(data.c_str(),data.size());

    //恢复cout的缓冲区
    std::cout.rdbuf(old_buf);
    ASSERT_EQ(oss.str(),data);
}

//测试FileFlush成功路径(单次读写)
TEST_F(LogFlushTest,FileFlush_success_test_single_write)
{
    json_data.flush_log_=2;
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    //不关心传入值，返回值返回kFakeFile
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    
    std::string data="hello world";

    //模拟一次写入全部数据
    EXPECT_CALL(*m,fwrite(data.c_str(),1,data.size(),kFakeFile)).Times(1).WillOnce(Return(data.size()));

    EXPECT_CALL(*m,fflush(kFakeFile)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*m,fileno(kFakeFile)).WillOnce(Return(12345));
    EXPECT_CALL(*m,fsync(12345)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));

    //创建实例对象
    auto file_flush=LogFlushFactory<FileFlush>::createLogFlush("place holder",json_data,std::move(mock));
    file_flush->flush(data.c_str(),data.size());
}

//测试FileFlush成功路径(多次读写)
TEST_F(LogFlushTest,FileFlush_success_test_mutiple_write)
{
    json_data.flush_log_=2;
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    //不关心传入值，返回值返回kFakeFile
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    
    std::string data="hello world";

    //模拟多次写入全部数据
    EXPECT_CALL(*m,fwrite(_,_,_,kFakeFile)).Times(3).WillOnce(Return(data.size()/2)).WillOnce(Return(data.size()/2)).WillOnce(Return(1));

    EXPECT_CALL(*m,fflush(kFakeFile)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*m,fileno(kFakeFile)).WillOnce(Return(12345));
    EXPECT_CALL(*m,fsync(12345)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));

    //创建实例对象
    auto file_flush=LogFlushFactory<FileFlush>::createLogFlush("place holder",json_data,std::move(mock));
    file_flush->flush(data.c_str(),data.size());
}

TEST_F(LogFlushTest,FileFlush_fail_test_fopen)
{
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();
    EXPECT_CALL(*m,createDirectory(_));

    //设置fopen失败
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(nullptr));
    //验证perror内容
    EXPECT_CALL(*m,perror(::testing::HasSubstr("fopen failed"))).Times(1);
    auto file_flush=LogFlushFactory<FileFlush>::createLogFlush("place holder",json_data,std::move(mock));
}

TEST_F(LogFlushTest,FileFlush_fail_test_fwrite_and_ferror)
{
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    //不关心传入值，返回值返回kFakeFile
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    
    std::string data="hello world";

    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));


    //设置fwrite失败
    EXPECT_CALL(*m,fwrite(_,_,_,kFakeFile)).Times(1).WillOnce(Return(0));
    //设置ferror成功
    EXPECT_CALL(*m,ferror(kFakeFile)).Times(1).WillOnce(Return(1));
    EXPECT_CALL(*m,perror(::testing::HasSubstr("fwrite failed"))).Times(1);

    //理论上fflush不会调用，因为提前返回了
    EXPECT_CALL(*m,fflush(kFakeFile)).Times(0);

    //创建实例对象
    auto file_flush=LogFlushFactory<FileFlush>::createLogFlush("place holder",json_data,std::move(mock));
    file_flush->flush(data.c_str(),data.size());
}

TEST_F(LogFlushTest,FileFlush_fail_test_fflush)
{
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    std::string data="hello world";
    EXPECT_CALL(*m,fwrite(data.c_str(),1,data.size(),kFakeFile)).Times(1).WillOnce(Return(data.size()));
    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));

    //设置fflush失败
    EXPECT_CALL(*m,fflush(kFakeFile)).Times(1).WillOnce(Return(-1));
    EXPECT_CALL(*m,fsync(_)).Times(0);
    EXPECT_CALL(*m,perror(::testing::HasSubstr("fflush failed"))).Times(1);


    //创建实例对象
    auto file_flush=LogFlushFactory<FileFlush>::createLogFlush("place holder",json_data,std::move(mock));
    file_flush->flush(data.c_str(),data.size());
}

TEST_F(LogFlushTest,FileFlush_fail_test_fsync)
{
    json_data.flush_log_=2;
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    std::string data="hello world";
    EXPECT_CALL(*m,fwrite(data.c_str(),1,data.size(),kFakeFile)).Times(1).WillOnce(Return(data.size()));
    EXPECT_CALL(*m,fileno(kFakeFile)).Times(1).WillOnce(Return(12345));
    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*m,fflush(kFakeFile)).Times(1).WillOnce(Return(0));


    //设置fsync失败
    EXPECT_CALL(*m,fsync(_)).Times(1).WillOnce(Return(-1));
    EXPECT_CALL(*m,perror(::testing::HasSubstr("fsync failed"))).Times(1);

    //创建实例对象
    auto file_flush=LogFlushFactory<FileFlush>::createLogFlush("place holder",json_data,std::move(mock));
    file_flush->flush(data.c_str(),data.size());
}

//测试RollFileFlush正常路径
TEST_F(LogFlushTest,RollFileFlush_success_test)
{
    json_data.flush_log_=2;
    std::string data="hello world";
    size_t max_size=data.size()-2;
    

    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();


    EXPECT_CALL(*m,fopen(::testing::MatchesRegex("log/LOG_.*_.*.log"),_)).Times(2).WillOnce(Return(kFakeFile)).WillOnce(Return(kFakeFile));
    EXPECT_CALL(*m,now()).Times(2).WillOnce(Return(1600000000)).WillOnce(Return(1700000000));

    EXPECT_CALL(*m,fwrite(data.c_str(),1,data.size(),kFakeFile)).Times(2).WillRepeatedly(Return(data.size()));

    EXPECT_CALL(*m,createDirectory(_));
    EXPECT_CALL(*m,fflush(kFakeFile)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(*m,fileno(kFakeFile)).Times(2).WillRepeatedly(Return(12345));
    EXPECT_CALL(*m,fsync(12345)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(*m,fclose(kFakeFile)).Times(2).WillRepeatedly(Return(0));

    //创建实例对象
    auto roll_file_flush=std::dynamic_pointer_cast<RollFileFlush>
                        (LogFlushFactory<RollFileFlush>::createLogFlush("log",max_size,json_data,std::move(mock)));
    
    ASSERT_NE(roll_file_flush,nullptr);
    roll_file_flush->flush(data.c_str(),data.size());
    ASSERT_EQ(roll_file_flush->getFileNum(),1);
    roll_file_flush->flush(data.c_str(),data.size());
    ASSERT_EQ(roll_file_flush->getFileNum(),2);
}

//测试RollFileFlush的runtime_error
TEST_F(LogFlushTest,RollFileFlush_fail_test_init_runtime_error)
{
    std::string data="hello world";
    size_t max_size=data.size()-2;

    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,fwrite(data.c_str(),1,data.size(),kFakeFile)).Times(0);

    //配置fopen失败
    EXPECT_CALL(*m,fopen(_,_)).Times(1).WillOnce(Return((FILE*)NULL));

    //创建实例对象
    auto roll_file_flush=std::dynamic_pointer_cast<RollFileFlush>
                        (LogFlushFactory<RollFileFlush>::createLogFlush("log",max_size,json_data,std::move(mock)));
    
    try
    {
        roll_file_flush->flush(data.c_str(),data.size());

        FAIL()<<"Expected runtime_error but no exception was thrown ";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_THAT(e.what(),::testing::HasSubstr("new file open failed "));
    }
    catch(...)
    {
        FAIL()<<"expected a runtime_error fail but caught a different exception";
    }
}



//测试异常
TEST_F(LogFlushTest,RollFileFlush_fail_test_fwrite_and_ferror)
{
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    //不关心传入值，返回值返回kFakeFile
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    
    std::string data="hello world";

    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));


    //设置fwrite失败
    EXPECT_CALL(*m,fwrite(_,_,_,kFakeFile)).Times(1).WillOnce(Return(0));
    //设置ferror成功
    EXPECT_CALL(*m,ferror(kFakeFile)).Times(1).WillOnce(Return(1));
    EXPECT_CALL(*m,perror(::testing::HasSubstr("fwrite failed"))).Times(1);

    //理论上fflush不会调用，因为提前返回了
    EXPECT_CALL(*m,fflush(kFakeFile)).Times(0);

    //创建实例对象
    auto roll_file_flush=LogFlushFactory<RollFileFlush>::createLogFlush("place holder",33,json_data,std::move(mock));
    roll_file_flush->flush(data.c_str(),data.size());
}

TEST_F(LogFlushTest,RollFileFlush_fail_test_fflush)
{
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    std::string data="hello world";
    EXPECT_CALL(*m,fwrite(data.c_str(),1,data.size(),kFakeFile)).Times(1).WillOnce(Return(data.size()));
    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));

    //设置fflush失败
    EXPECT_CALL(*m,fflush(kFakeFile)).Times(1).WillOnce(Return(-1));
    EXPECT_CALL(*m,fsync(_)).Times(0);
    EXPECT_CALL(*m,perror(::testing::HasSubstr("fflush failed"))).Times(1);


    //创建实例对象
    auto roll_file_flush=LogFlushFactory<RollFileFlush>::createLogFlush("place holder",33,json_data,std::move(mock));
    roll_file_flush->flush(data.c_str(),data.size());
}

TEST_F(LogFlushTest,RollFileFlush_fail_test_fsync)
{
    json_data.flush_log_=2;
    auto mock=std::make_unique<MockSystemOps>();
    MockSystemOps* m=mock.get();

    EXPECT_CALL(*m,createDirectory(_));
    EXPECT_CALL(*m,fopen(_,_)).WillOnce(Return(kFakeFile));
    std::string data="hello world";
    EXPECT_CALL(*m,fwrite(data.c_str(),1,data.size(),kFakeFile)).Times(1).WillOnce(Return(data.size()));
    EXPECT_CALL(*m,fileno(kFakeFile)).Times(1).WillOnce(Return(12345));
    EXPECT_CALL(*m,fclose(kFakeFile)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*m,fflush(kFakeFile)).Times(1).WillOnce(Return(0));


    //设置fsync失败
    EXPECT_CALL(*m,fsync(_)).Times(1).WillOnce(Return(-1));
    EXPECT_CALL(*m,perror(::testing::HasSubstr("fsync failed"))).Times(1);

    //创建实例对象
    auto roll_file_flush=LogFlushFactory<RollFileFlush>::createLogFlush("place holder",33,json_data,std::move(mock));
    roll_file_flush->flush(data.c_str(),data.size());
}











