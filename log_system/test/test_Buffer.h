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
        json_data.threshold_=20;
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


    std::string data2(3,'1');
    std::string data3(18,'3');
    std::string data4(4,'4');
    auto tem_conf=json_data;
    //测试线性扩容
    tem_conf.buffer_size_=20;
    buf.reset();
    buf=std::make_unique<Buffer>(tem_conf);
    buf->push(data3.c_str(),data3.size());
    buf->push(data2.c_str(),data2.size());
    ASSERT_EQ(buf->size(),26);
    ASSERT_EQ(buf->writeableBytes(),5);
    ASSERT_EQ(buf->peek(),data3+data2);

    buf.reset();
    buf=std::make_unique<Buffer>(tem_conf);
    buf->push(data3.c_str(),data3.size());
    buf->push(data4.c_str(),data4.size());
    ASSERT_EQ(buf->size(),26);
    ASSERT_EQ(buf->writeableBytes(),4);
    ASSERT_EQ(buf->peek(),data3+data4);

    //测试成倍扩容
    buf.reset();
    buf=std::make_unique<Buffer>(json_data);
    buf->push(data.c_str(),data.size());
    buf->push(data2.c_str(),data2.size());
    ASSERT_EQ(buf->size(),20);
    ASSERT_EQ(buf->writeableBytes(),9);
    ASSERT_EQ(buf->peek(),data+data2);
    
    buf.reset();
    buf=std::make_unique<Buffer>(json_data);
    buf->push(data.c_str(),data.size());
    buf->push(data3.c_str(),data3.size());
    ASSERT_EQ(buf->size(),40);
    ASSERT_EQ(buf->writeableBytes(),14);
    ASSERT_EQ(buf->peek(),data+data3);
}







