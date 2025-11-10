#pragma once

#include "test_helper.h"
#include "Util.hpp"
#include <sys/types.h>
#include <filesystem>
#include <chrono>

class UtilTest: public ::testing::Test
{
protected:
    inline static std::string file_name;
    inline static std::string file_content;

    static void SetUpTestSuite()
    {
        file_name="./get_file.txt";
        file_content="hello world, this is a test";

        std::ofstream ofs(file_name);
        ASSERT_TRUE(ofs.is_open());
        ofs<<file_content;
        ofs.close();
    }
    static void TearDownTestSuite()
    {
        std::filesystem::remove(file_name);
    }
};

TEST_F(UtilTest,date_test)
{
    auto time_=asynclog::Util::Date::now();
    auto std_time_=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ASSERT_NEAR(time_,std_time_,1);
}

TEST_F(UtilTest,create_dictory_test)
{
    using namespace asynclog::Util::File;
    std::string path_name1="./test1/test01";
    std::string path_name2="./test1/test02";
    std::string path_name3="./test1/../test1";
    std::string path_name4="./test1/..";
    std::string path_name5="./../test/test2";
    std::string_view path_name2_sv(path_name2);

    ASSERT_FALSE(exists(path_name1));
    ASSERT_FALSE(exists(path_name2));

    //创建目录
    createDictory(path_name1);
    createDictory(path_name2);
    createDictory(path_name3);
    createDictory(path_name5);
    
    ASSERT_TRUE(exists(path_name1));
    ASSERT_TRUE(exists(path_name2));
    ASSERT_TRUE(exists(path_name3));
    ASSERT_TRUE(exists(path_name2_sv));
    
    rmdir(path_name1.c_str());
    rmdir(path_name2.c_str());
    rmdir(path_name5.c_str());
    rmdir(path_name3.c_str());
}

TEST_F(UtilTest,get_file_size_test)
{
    using namespace asynclog::Util::File;

    std::ifstream ifs(file_name,std::ios::ate);
    ASSERT_TRUE(ifs.is_open());

    auto file_len=ifs.tellg();
    ASSERT_TRUE(file_len!=-1);

    ASSERT_EQ(file_len,fileSize(file_name));
}

TEST_F(UtilTest,get_file_content_test)
{
    using namespace asynclog::Util::File;
    std::string content;
    
    bool is_success=getFileContent(content,file_name);
    ASSERT_TRUE(is_success);

    ASSERT_EQ(content,file_content);
}