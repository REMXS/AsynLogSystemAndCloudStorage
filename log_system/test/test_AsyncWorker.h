#pragma once

#include "test_helper.h"
#include <thread>
#include <vector>


#include "AsyncWorker.hpp"


using namespace asynclog;
class AsyncWorkerTest: public ::testing::Test
{
protected:
    void SetUp()override
    {
        json_data.buffer_size_=10;
    }
    void TearDown()override
    {

    }

    void dataProcess(Buffer&buf)
    {
        char* data=buf.readBegin(buf.readableBytes());
        if(data) output_buffer+=std::string(data,buf.readableBytes());
        buf.moveReadPos(buf.readableBytes());
    }

    Util::JsonUtil::JsonData json_data;
    std::string output_buffer;//消费者最终都将数据输出到这里
};

//测试基础读写
TEST_F(AsyncWorkerTest,out_put_test)
{
    json_data.buffer_size_=15;
    std::string data="hello world";
    AsyncWorker worker(json_data,[this](Buffer&buf){dataProcess(buf);});
    worker.start();
    bool is=worker.push(data.c_str(),data.size());
    ASSERT_TRUE(is);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_EQ(data,output_buffer);

    
}

//测试超时刷新
TEST_F(AsyncWorkerTest,time_out_flush_test)
{
    json_data.buffer_size_=15;
    AsyncWorker worker(json_data,[this](Buffer&buf){dataProcess(buf);});
    worker.start();
    std::string data2="1";
    bool is=worker.push(data2.c_str(),data2.size());
    ASSERT_TRUE(is);

    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    ASSERT_EQ(data2,output_buffer);
}


//测试过量写入
TEST_F(AsyncWorkerTest,limited_size_over_write_test)
{
    json_data.buffer_size_=7;
    std::string data="hello world";
    AsyncWorker worker(json_data,[this](Buffer&buf){dataProcess(buf);},BufferPolicy::LIMIT_SIZE,7);
    worker.start();
    bool is=worker.push(data.c_str(),data.size());
    ASSERT_FALSE(is);
}

//测试析构的时候是否会把剩余数据写入
TEST_F(AsyncWorkerTest,stop_test)
{
    std::string data="hello world";
    {
        json_data.buffer_size_=7;
        AsyncWorker worker(json_data,[this](Buffer&buf){dataProcess(buf);});
        worker.start();
        bool is=worker.push(data.c_str(),data.size());
        ASSERT_TRUE(is);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_EQ(data,output_buffer);
}

//多线程测试
TEST_F(AsyncWorkerTest,mutiple_thread_test)
{
    std::vector<std::string>datas;
    std::vector<std::thread>threads;
    AsyncWorker worker(json_data,[this](Buffer&buf){dataProcess(buf);});
    worker.start();

    //准备数据
    for(int i=0;i<100;++i)
    {
        datas.emplace_back(std::string(15,i));
    }
    auto thread_func=[&](int idx){
        for(int i=idx;i<idx+10;++i)
        {
            ASSERT_GT(datas.size(),i);
            worker.push(datas[i].c_str(),datas[i].size());
        }
    };

    //写入数据
    for(int i=0;i<10;++i)
    {
        threads.emplace_back(thread_func,i*10);
    }
    for(auto&t:threads)
    {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    //验证数据
    for(auto&data:datas)
    {
        EXPECT_THAT(output_buffer,::testing::HasSubstr(data));
    }
}



