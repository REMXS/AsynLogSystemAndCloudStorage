#pragma once
#include "../test_helper.h"
#include "../../src/server/Util.hpp"


namespace fs=std::filesystem;

class UtilTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        //在临时目录下创建一个临时文件
        temp_file_path=fs::temp_directory_path()/fs::path("util_test_temp_file.txt");
        file_content="This is a test file for UtilTest.\nIt contains multiple lines of text.\n1234567890\n!@#$%^&*()_+\n";
        std::ofstream ofs(temp_file_path,std::ios::binary);
        ofs<<file_content;
        ofs.close();
    }

    void TearDown() override
    {
        //删除临时文件
        if(fs::exists(temp_file_path))
        {
            fs::remove(temp_file_path);
        }
    }

    fs::path temp_file_path;
    std::string file_content;
};

TEST_F(UtilTest, ToHex_test)
{
    EXPECT_EQ(mystorage::toHex(0), '0');
    EXPECT_EQ(mystorage::toHex(9), '9');
    EXPECT_EQ(mystorage::toHex(10), 'A');
    EXPECT_EQ(mystorage::toHex(15), 'F');
}

TEST_F(UtilTest, FromHex_test)
{
    EXPECT_EQ(mystorage::fromHex('0'), 0);
    EXPECT_EQ(mystorage::fromHex('9'), 9);
    EXPECT_EQ(mystorage::fromHex('a'), 10);
    EXPECT_EQ(mystorage::fromHex('e'), 14);
    EXPECT_EQ(mystorage::fromHex('A'), 10);
    EXPECT_EQ(mystorage::fromHex('E'), 14);

    EXPECT_THROW(mystorage::fromHex('g'), std::invalid_argument);
    EXPECT_THROW(mystorage::fromHex('Z'), std::invalid_argument);
    EXPECT_THROW(mystorage::fromHex('%'), std::invalid_argument);
}


TEST_F(UtilTest, UrlDecode_test)
{
    std::string encoded="Hello%20World%21%20This%20is%20a%20test.%20%40%23%24%25%5E%26*()";
    std::string expected="Hello World! This is a test. @#$%^&*()";

    std::string decoded=mystorage::urlDecode(encoded);
    EXPECT_EQ(decoded, expected);
}

TEST_F(UtilTest, FileSize_test)
{
    mystorage::FileUtil file_util(temp_file_path.string());
    EXPECT_EQ(file_util.getFileSize(), file_content.size());
}

TEST_F(UtilTest, FileSize_fail_test)
{
    mystorage::FileUtil file_util("non_existent_file.txt");
    EXPECT_FALSE(file_util.getFileSize()>=0);
}


