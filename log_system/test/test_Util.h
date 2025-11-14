#pragma once

#include "test_helper.h"
#include "Util.hpp"
#include <sys/types.h>
#include <chrono>
#include <vector>

namespace fs = std::filesystem;

class UtilTest: public ::testing::Test
{
protected:
    inline static fs::path test_dir;
    inline static fs::path test_file;
    inline static std::string file_content;

    static void SetUpTestSuite()
    {
        test_dir = fs::temp_directory_path()/"UtilTestDir";
        test_file = test_dir / "get_file.txt";
        file_content="hello world, this is a test";

        //确保test_dir为新的
        fs::remove_all(test_dir);
        fs::create_directories(test_dir);

        //向测试文件中写入数据
        std::ofstream ofs(test_file);
        ASSERT_TRUE(ofs.is_open());
        ofs<<file_content;
        ofs.close();
    }

    static void TearDownTestSuite()
    {
        //递归删除所有测试文件夹中的内容和测试文件夹
        std::filesystem::remove_all(test_dir);
    }
};

TEST_F(UtilTest,date_test)
{
    auto time_=asynclog::Util::Date::now();
    auto std_time_=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ASSERT_NEAR(time_,std_time_,1);
}

TEST_F(UtilTest,create_directory_test)
{
    using namespace asynclog::Util::File;
    fs::path path1 = test_dir / "test1" / "test01";
    fs::path path2 = test_dir / "test1" / "test02";
    fs::path path3 = test_dir / "test1" / ".." / "test1"; // 等价于 test_dir / "test1"
    fs::path path4 = test_dir / "test_already_exists";

    ASSERT_FALSE(exists(path1.string()));
    ASSERT_FALSE(exists(path2.string()));

    createDirectory(path1.string());
    createDirectory(path2.string());
    createDirectory(path3.string());

    fs::create_directory(path4);
    ASSERT_TRUE(exists(path4.string()));
    createDirectory(path4.string());

    // 结果验证
    ASSERT_TRUE(exists(path1.string()));
    ASSERT_TRUE(exists(path2.string()));
    ASSERT_TRUE(exists(path3.string()));
    ASSERT_TRUE(exists(path4.string()));

    //测试string_view 重载
    std::string temp_path1=path1.string();
    std::string_view path1_sv(temp_path1);
    ASSERT_TRUE(exists(path1_sv));
}

TEST_F(UtilTest,get_file_size_test)
{
    using namespace asynclog::Util::File;

    ASSERT_EQ(file_content.size(),fileSize(test_file.string()));
}

TEST_F(UtilTest,get_file_content_test)
{
    using namespace asynclog::Util::File;
    std::string content;
    
    bool is_success=getFileContent(content,test_file.string());
    ASSERT_TRUE(is_success);

    ASSERT_EQ(content,file_content);
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

TEST_F(UtilTest,config_test_default)
{
    using namespace asynclog::Util;

    std::string config_content=R"({
    "buffer_size": 10000000,       
    "threshold": 10000000000,      
    "linear_growth" : 10000000,
    "flush_log" : 2,
    "backup_addr" : "47.116.74.254",
    "backup_port" : 8080,
    "thread_count" : 3
})";
    fs::path conf_path="./log_config.conf";
    {
        std::ofstream ofs(conf_path);
        ASSERT_TRUE(ofs.is_open());
        ofs.write(config_content.c_str(),config_content.size());
        ofs.close();
    }

    JsonUtil::JsonData default_config;
    JsonUtil::JsonData j_config;
    j_config.loadConfig(conf_path.string());
    default_config.loadConfig("sfsdf");

    ASSERT_EQ(j_config.buffer_size_,10000000);
    ASSERT_EQ(j_config.threshold_,10000000000);
    ASSERT_EQ(j_config.linear_growth_,10000000);
    ASSERT_EQ(j_config.flush_log_,2);
    ASSERT_EQ(j_config.backup_addr_,"47.116.74.254");
    ASSERT_EQ(j_config.backup_port_,8080);
    ASSERT_EQ(j_config.thread_count_,3);

    ASSERT_EQ(default_config.buffer_size_,4 * 1024 * 1024);
    ASSERT_EQ(default_config.threshold_,1024);
    ASSERT_EQ(default_config.linear_growth_,1024 * 1024);
    ASSERT_EQ(default_config.flush_log_,1);
    ASSERT_EQ(default_config.backup_addr_,"127.0.0.1");
    ASSERT_EQ(default_config.backup_port_,8080);
    ASSERT_EQ(default_config.thread_count_,1);

    fs::remove(conf_path);
}
