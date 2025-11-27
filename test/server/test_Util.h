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

//访问文件相关的测试
TEST_F(UtilTest,lastModifyTime_test)
{
    mystorage::FileUtil file_util(temp_file_path);
    auto mod_time=file_util.lastModifyTime();
    EXPECT_NEAR(mod_time,time(nullptr),5);
}

TEST_F(UtilTest,lastAccessTime_test)
{
    mystorage::FileUtil file_util(temp_file_path);
    auto access_time=file_util.lastAccessTime();
    EXPECT_NEAR(access_time,time(nullptr),5);
}

TEST_F(UtilTest,lastModifyTime_fail_test)
{
    mystorage::FileUtil file_util("non_existent_file.txt");
    auto mod_time=file_util.lastModifyTime();
    EXPECT_EQ(mod_time,-1);
}

TEST_F(UtilTest,lastAccessTime_fail_test)
{
    mystorage::FileUtil file_util("non_existent_file.txt");
    auto access_time=file_util.lastAccessTime();
    EXPECT_EQ(access_time,-1);
}

TEST_F(UtilTest,getPosLen_test)
{
    std::ifstream ifs(temp_file_path,std::ios::binary);
    std::string buf;
    
    //正常读取测试
    bool res=mystorage::FileUtil::getPosLen(ifs,buf,10,20);
    ASSERT_TRUE(res);
    EXPECT_EQ(buf,file_content.substr(10,20));

    //边沿读取测试
    res=mystorage::FileUtil::getPosLen(ifs,buf,10,1000);
    ASSERT_TRUE(res);
    EXPECT_EQ(buf,std::string(file_content.begin()+10,file_content.end()));
    //判断触发eof
    ASSERT_FALSE(ifs.good());

    //判断触发eof后依然可以正常读取
    res=mystorage::FileUtil::getPosLen(ifs,buf,10,20);
    ASSERT_TRUE(res);
    EXPECT_EQ(buf,file_content.substr(10,20));

    ifs.close();
}

TEST_F(UtilTest,getPosLen_fail_test)
{
    using namespace::mystorage;
    std::ifstream ifs;
    std::string buf;

    //测试异常数值
    bool res=FileUtil::getPosLen(ifs,buf,-1,3);
    ASSERT_FALSE(res);
    res=FileUtil::getPosLen(ifs,buf,2,3);
    ASSERT_FALSE(res);

    //测试文件打开失败
    res=FileUtil::getPosLen(ifs,buf,0,34);
    ASSERT_FALSE(res);

    ifs.open(temp_file_path,std::ios::binary);
    //测试文件过量移动指针导致的失败
    res=FileUtil::getPosLen(ifs,buf,100,2);
    ASSERT_FALSE(res);
}

TEST_F(UtilTest,getFileContent_test)
{
    mystorage::FileUtil file_util(temp_file_path);
    std::string buf;
    bool res=file_util.getFileContent(buf);
    ASSERT_TRUE(res);
    EXPECT_EQ(buf,file_content);
}

TEST_F(UtilTest,getFileName_test)
{
    mystorage::FileUtil file_util(temp_file_path);
    EXPECT_EQ(file_util.getFileName(), "util_test_temp_file.txt");
}

TEST_F(UtilTest,setContent_test)
{
    mystorage::FileUtil file_util(temp_file_path);
    std::string new_content="New content for testing setContent method.";
    bool res=file_util.setContent(new_content.data(),new_content.size());
    ASSERT_TRUE(res);

    //验证内容是否写入成功
    std::string buf;
    res=file_util.getFileContent(buf);
    ASSERT_TRUE(res);
    EXPECT_EQ(buf,new_content);
}
TEST_F(UtilTest,setContent_fail_test)
{
    mystorage::FileUtil fake_file_util("/protected_file.txt"); 
    std::string new_content="Trying to write to a protected file.";

    //测试打开文件失败
    bool res=fake_file_util.setContent(new_content.data(),new_content.size());
    ASSERT_FALSE(res);

    //测试文件写入失败
    mystorage::FileUtil file_util(temp_file_path);

    bool res2=file_util.setContent(nullptr,10);
    ASSERT_FALSE(res2);
}
