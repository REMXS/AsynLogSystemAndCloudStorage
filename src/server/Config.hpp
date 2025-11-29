#pragma once

#include <memory>
#include <filesystem>
#include <jsoncpp/json/json.h>


#include "Util.hpp"


namespace mystorage
{
class Config
{
private:
    uint16_t server_port_;
    std::string server_ip_;
    std::string download_prefix_; // URL路径前缀
    std::string deep_storage_dir_;     // 深度存储文件的存储路径
    std::string low_storage_dir_;     // 浅度存储文件的存储路径
    std::string storage_info_;     // sqlite3 数据库文件
public:
    Config()
        :server_port_(0)
    {}
    ~Config()=default;

    //读取配置文件
    void readConfig(fs::path config_path)
    {
        FileUtil file_util(config_path);
        std::string config_content;

        bool is=file_util.getFileContent(config_content);
        if(!is)
        {
            throw std::invalid_argument("failed to get config file!");
        }
        
        Json::Value root;
        is=JsonUtil::parse(config_content,root);
        if(!is)
        {
            throw std::invalid_argument("read config failed!");
        }

        server_port_=root["server_port"].asUInt();
        server_ip_=root["server_ip"].asString();
        download_prefix_=root["download_prefix"].asString();
        deep_storage_dir_=root["deep_storage_dir"].asString();
        low_storage_dir_=root["low_storage_dir"].asString();
        storage_info_=root["storage_info"].asString();

        if(!server_port_||server_ip_.empty()||download_prefix_.empty()||deep_storage_dir_.empty()||
            low_storage_dir_.empty()||storage_info_.empty())
        {
            throw std::invalid_argument("there are some invalid arguments");
        }
    }

    uint16_t getServerPort() const 
    {
        return server_port_;
    }
    std::string getServerIp() const
    {
        return server_ip_;
    }
    std::string getDownloadPrefix() const
    {
        return download_prefix_;
    }
    std::string getDeepStorageDir() const
    {
        return deep_storage_dir_;
    }
    std::string getLowStorageDir() const
    {
        return low_storage_dir_;
    }
    std::string getStorageInfoFile() const
    {
        return storage_info_;
    }

    void setServerPort(uint16_t port)
    {
        server_port_=port;
    }
    void setServerIp(const std::string& ip)
    {
        server_ip_=ip;
    }
    void setDownloadPrefix(const std::string& prefix)
    {
        download_prefix_=prefix;
    }
    void setDeepStorageDir(const std::string& dir)
    {
        deep_storage_dir_=dir;
    }
    void setLowStorageDir(const std::string& dir)
    {
        low_storage_dir_=dir;
    }
    void setStorageInfoFile(const std::string& file)
    {
        storage_info_=file;
    }
};

} // namespace mystorage
