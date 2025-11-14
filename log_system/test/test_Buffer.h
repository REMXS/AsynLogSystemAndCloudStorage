#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <memory>

#include "test_helper.h"
#include "AsyncBuffer.hpp"

namespace fs = std::filesystem;

class BufferTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        json_data.buffer_size_=10;
        json_data.threshold_=5;
        json_data.linear_growth_=3;
    }

    void SetUp()override
    {
        buf=std::make_unique<asynclog::Buffer>(json_data);
    }

    inline static asynclog::Util::JsonUtil::JsonData json_data;

    std::unique_ptr<asynclog::Buffer> buf;
};

TEST_F(BufferTest,base_test)
{
    ASSERT_TRUE(buf->isEmpty());
    ASSERT_EQ(buf->readBegin(33),nullptr);
    ASSERT_EQ(buf->readableBytes(),0);
    ASSERT_EQ(buf->writeableBytes(),10);
    
    //测试无效操作
    buf->moveReadPos(32);
    ASSERT_EQ(buf->readableBytes(),0);

    //移动指针
    buf->moveWritePos(5);
    buf->moveReadPos(3);
    ASSERT_EQ(buf->readableBytes(),2);
    ASSERT_EQ(buf->writeableBytes(),5);

    ASSERT_FALSE(buf->isEmpty());
    ASSERT_EQ(buf->readBegin(33),nullptr);
    ASSERT_NE(buf->readBegin(1),nullptr);

    buf->moveWritePos(1121);
    ASSERT_EQ(buf->writeableBytes(),0);
}

TEST_F(BufferTest,push_test)
{
    std::string data="123";
    buf->push(data.c_str(),data.size());
    ASSERT_EQ(buf->writeableBytes(),json_data.buffer_size_-data.size());
    ASSERT_EQ(buf->peek(),data);
    buf->moveReadPos(2);
    ASSERT_EQ(buf->peek(),std::string(data.begin()+2,data.end()));
    const char* tem=buf->readBegin(1);
    ASSERT_EQ(buf->peek(),std::string(data.begin()+2,data.end()));
}

TEST_F(BufferTest,expansion_test)
{
    using namespace asynclog;

    std::string data="12345678";
    buf->push(data.c_str(),data.size());

    //测试移动可读区域，不进行扩容
    buf->moveReadPos(data.size());
    buf->push(data.c_str(),data.size());
    ASSERT_EQ(buf->writeableBytes(),2);
    ASSERT_EQ(buf->size(),10);

    //测试线性扩容
    std::string data1(3,'0');
    buf->push(data1.c_str(),data1.size());
    ASSERT_EQ(buf->writeableBytes(),5);
    ASSERT_EQ(buf->size(),16);
    ASSERT_EQ(buf->peek(),data+data1);

    buf.reset();
    buf=std::make_unique<Buffer>(json_data);
    data1="0000";
    buf->push(data.c_str(),data.size());
    buf->push(data1.c_str(),data1.size());
    ASSERT_EQ(buf->writeableBytes(),4);
    ASSERT_EQ(buf->size(),16);
    ASSERT_EQ(buf->peek(),data+data1);

    //测试成倍扩容
    buf.reset();
    buf=std::make_unique<Buffer>(json_data);
    data1=std::string(6,'0');
    buf->push(data.c_str(),data.size());
    buf->push(data1.c_str(),data1.size());
    ASSERT_EQ(buf->writeableBytes(),6);
    ASSERT_EQ(buf->size(),20);
    ASSERT_EQ(buf->peek(),data+data1);

    buf.reset();
    buf=std::make_unique<Buffer>(json_data);
    data1=std::string(16,'0');
    buf->push(data.c_str(),data.size());
    buf->push(data1.c_str(),data1.size());
    ASSERT_EQ(buf->writeableBytes(),16);
    ASSERT_EQ(buf->size(),40);
    ASSERT_EQ(buf->peek(),data+data1);
}







