#pragma once

#include "../test_helper.h"
#include "../../src/server/DataManager.hpp"

using namespace mystorage;

class DataManagerTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        std::ofstream ofs(test_db_file_,std::ios::out);
        ofs.close();
    }
    static void TearDownTestSuite()
    {
        //删除测试数据库文件
        fs::remove(test_db_file_);
    }

    void SetUp() override
    {
        test_config_.setDownloadPrefix("download/");
        test_config_.setStorageInfoFile(test_db_file_);

        test_storage_info_=std::make_unique<StorageInfo>(test_config_);
        test_storage_info_->newStorageInfo("test_file.txt");
        test_storage_info_->url_="download/test_file.txt";
        test_storage_info_->storage_path_="test_file.txt";
        test_storage_info_->file_size_=20;
    }

    void TearDown() override
    {
    }


    Config test_config_;
    inline static const std::string test_db_file_="test_storage_info.db";

    std::unique_ptr<StorageInfo> test_storage_info_;
};

TEST_F(DataManagerTest, NewStorageInfoTest)
{
    StorageInfo info(test_config_);
    //创建测试文件
    fs::path test_file_path="test_file.txt";
    {
        std::ofstream ofs(test_file_path,std::ios::out);
        ofs<<"This is a test file.";
        ofs.close();
    }
    

    bool is=info.newStorageInfo(test_file_path);
    EXPECT_TRUE(is);
    EXPECT_EQ(info.url_,"download/test_file.txt");
    EXPECT_EQ(info.storage_path_,test_file_path.string());
    EXPECT_EQ(info.file_size_,20);
    //清理测试文件
    fs::remove(test_file_path);
}

TEST_F(DataManagerTest, InsertAndGetTest)
{
    DataManager data_manager(test_config_);
    //插入数据
    bool is=data_manager.insert(*test_storage_info_);
    EXPECT_TRUE(is);

    //通过URL获取数据
    StorageInfo fetched_info;
    is=data_manager.getOneByURL(test_storage_info_->url_,fetched_info);
    EXPECT_TRUE(is);
    EXPECT_EQ(fetched_info.url_,test_storage_info_->url_);
    EXPECT_EQ(fetched_info.storage_path_,test_storage_info_->storage_path_);
    EXPECT_EQ(fetched_info.file_size_,test_storage_info_->file_size_);
    //通过存储路径获取数据
    StorageInfo fetched_info_by_sp;
    is=data_manager.getOneByStoragePath(test_storage_info_->storage_path_,fetched_info_by_sp);
    EXPECT_TRUE(is);
    EXPECT_EQ(fetched_info_by_sp.url_,test_storage_info_->url_);
    EXPECT_EQ(fetched_info_by_sp.storage_path_,test_storage_info_->storage_path_);
    EXPECT_EQ(fetched_info_by_sp.file_size_,test_storage_info_->file_size_);
}

TEST_F(DataManagerTest, DeleteByURLTest)
{
    DataManager data_manager(test_config_);

    //插入数据
    bool is=data_manager.insert(*test_storage_info_);
    EXPECT_TRUE(is);

    //删除数据
    is=data_manager.deleteByURL(test_storage_info_->url_);
    EXPECT_TRUE(is);

    //尝试通过URL获取数据，应该失败
    StorageInfo fetched_info;
    is=data_manager.getOneByURL(test_storage_info_->url_,fetched_info);
    EXPECT_TRUE(is);
    EXPECT_NE(fetched_info.url_,test_storage_info_->url_); //url 不应该相等
}
TEST_F(DataManagerTest, GetAllTest)
{
    DataManager data_manager(test_config_);

    StorageInfo info2(test_config_);

    //插入数据
    bool is=data_manager.insert(*test_storage_info_);
    EXPECT_TRUE(is);
    is=data_manager.insert(info2);
    EXPECT_TRUE(is);

    //获取所有数据
    std::vector<StorageInfo> all_infos;
    is=data_manager.getAll(all_infos);
    EXPECT_TRUE(is);
    EXPECT_GE(all_infos.size(),2);
}

TEST_F(DataManagerTest, init_fail_test_)
{
    //使用不存在的数据库文件初始化DataManager，应该抛出异常
    Config invalid_config;
    invalid_config.setStorageInfoFile("non_existent_db_file.db");
    EXPECT_THROW(DataManager data_manager(invalid_config),std::invalid_argument);
}

TEST_F(DataManagerTest, create_table_fail_test_)
{
    //使用一个无效的数据库文件初始化DataManager，应该抛出异常
    //创建一个空文件
    std::string invalid_db_file="invalid_db_file.txt";
    {
        std::ofstream ofs(invalid_db_file,std::ios::out);
        ofs<<"This is not a valid database file.";
        ofs.close();
    }

    Config invalid_config;
    invalid_config.setStorageInfoFile(invalid_db_file);
    EXPECT_THROW(DataManager data_manager(invalid_config),std::runtime_error);

    //清理测试文件
    fs::remove(invalid_db_file);
}





