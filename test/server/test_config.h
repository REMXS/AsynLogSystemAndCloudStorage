#pragma once

#include "../test_helper.h"
#include "../../src/server/Config.hpp"

namespace fs=std::filesystem;

class ConfigTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        config_content=R"({
            "server_port" : 8081,
            "server_ip" : "127.0.0.1", 
            "download_prefix" : "/download/", 
            "deep_storage_dir" : "./deep_storage/",   
            "low_storage_dir" : "./low_storage/", 
            "bundle_format":4,
            "storage_info" : "./storage.data"
        })";

        config_path="./server_config.conf";

        //写入配置文件
        std::ofstream ofs(config_path,std::ios::binary);
        ofs<<config_content;
        ofs.close();

    }
    static void TearDownTestSuite()
    {
        //删除配置文件
        if(fs::exists(config_path))
        {
            fs::remove(config_path);
        }
    }

    inline static fs::path config_path;
    inline static std::string config_content;
};

TEST_F(ConfigTest, readConfig_test)
{
    mystorage::Config config;
    EXPECT_NO_THROW(config.readConfig(config_path));

    EXPECT_EQ(config.getServerPort(), 8081);
    EXPECT_EQ(config.getServerIp(), "127.0.0.1");
    EXPECT_EQ(config.getDownloadPrefix(), "/download/");
    EXPECT_EQ(config.getDeepStorageDir(), "./deep_storage/");
    EXPECT_EQ(config.getLowStorageDir(), "./low_storage/");
    EXPECT_EQ(config.getStorageInfoFile(), "./storage.data");
}

TEST_F(ConfigTest, readConfig_invalid_test)
{
    mystorage::Config config;
    fs::path invalid_path="./invalid_config.conf";

    //写入无效配置文件
    std::string invalid_content=R"({
        "server_port" : 0,
        "server_ip" : "", 
        "download_prefix" : "/download/", 
        "deep_storage_dir" : "./deep_storage/",   
        "low_storage_dir" : "./low_storage/", 
        "storage_info" : "./storage.data"
    })";
    std::ofstream ofs(invalid_path,std::ios::binary);
    ofs<<invalid_content;
    ofs.close();

    EXPECT_THROW(config.readConfig(invalid_path), std::invalid_argument);

    fs::remove(invalid_path);
}

TEST_F(ConfigTest, readConfig_missing_test)
{
    mystorage::Config config;
    fs::path missing_path="./missing_config.conf";
    EXPECT_THROW(config.readConfig(missing_path), std::invalid_argument);
}




