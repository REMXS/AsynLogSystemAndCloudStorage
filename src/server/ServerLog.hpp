#pragma once
#include "MyLog.hpp"

namespace mystorage
{
const std::string SERVER_LOGGER_NAME = "cloud_storage_server";
std::shared_ptr<asynclog::ThreadPool>LOGGER_POOL;
std::shared_ptr<asynclog::AsyncLogger>_ptr;

inline void initServerLog()
{

    //初始化线程池
    LOGGER_POOL=std::make_shared<asynclog::ThreadPool>(2,100);
    asynclog::AsyncLoggerBuilder builder;

    //设置配置文件
    asynclog::Util::JsonUtil::JsonData config_data;
    config_data.loadConfig("./log_config.conf");


    builder.setLoggerName(SERVER_LOGGER_NAME);
    builder.setConfig(config_data);
    asynclog::Manager::getInstance().addLogger(builder.build(LOGGER_POOL));

    _ptr=asynclog::Manager::getInstance().getLogger(SERVER_LOGGER_NAME);
    
}

inline std::shared_ptr<asynclog::AsyncLogger> getLogger()
{
    return _ptr;
}

} //namespace mystorage