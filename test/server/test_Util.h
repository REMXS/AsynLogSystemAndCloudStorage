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

TEST_F(UtilTest,compress_uncompress_test)
{
    mystorage::FileUtil file_util(temp_file_path);

    //压缩内容到文件
    file_util.compress(file_content,3);
    file_util.uncompress(temp_file_path);

    std::string out_put_file;
    file_util.getFileContent(out_put_file);

    EXPECT_EQ(file_content,out_put_file);
}

TEST_F(UtilTest,scanDirectory_test)
{
    mystorage::FileUtil file_util(temp_file_path);
    std::vector<fs::path>out_put;
    EXPECT_FALSE(file_util.scanDirectory(out_put));
}

TEST_F(UtilTest,json_test)
{
    using namespace asynclog::Util;
    std::vector<std::pair<Json::Value,std::string>>vals(5); //json对象和其对应的字符串
    vals[0].first["name"] = "Alice";
    vals[0].first["age"] = 30;
    vals[0].first["isStudent"] = false;
    vals[0].second=R"({"name":"Alice","age":30,"isStudent":false})";

    vals[1].first.append("apple");
    vals[1].first.append("banana");
    vals[1].first.append("cherry");
    vals[1].second=R"(["apple","banana","cherry"])";

    vals[2].first["user"]["id"] = 123;
    vals[2].first["user"]["name"] = "Bob";
    vals[2].first["scores"].append(85);
    vals[2].first["scores"].append(92);
    vals[2].second=R"({"user":{"id":123,"name":"Bob"},"scores":[85,92]})";

    vals[3].first=Json::Value(Json::objectValue);
    vals[3].second=R"({})";


    vals[4].first["key1"] = "value";
    vals[4].first["key2"] = Json::nullValue;
    vals[4].second=R"({"key1":"value","key2":null})";

    for(auto&[json_val,str]:vals)
    {
        Json::Value ret_json_val;
        //验证从字符串转化为json对象
        JsonUtil::parse(str,ret_json_val);
        ASSERT_EQ(ret_json_val,json_val);

        //验证从json转换为字符串
        //因为直接比较字符串很困难，所以先把json转化为字符串，然后再将字符串转化为json来验证
        std::string ret_str;
        Json::Value tem_json;
        JsonUtil::serialize(json_val,ret_str);
        JsonUtil::parse(ret_str,tem_json);
        ASSERT_EQ(tem_json,json_val);
    }
}

