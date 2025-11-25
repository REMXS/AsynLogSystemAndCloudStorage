#pragma once
#include <unordered_map>

#include "AsyncLogger.hpp"

namespace asynclog
{

class Manager
{
private:
    std::unordered_map<std::string,std::shared_ptr<AsyncLogger>>loggers_;
    std::mutex mtx_;
    std::shared_ptr<AsyncLogger>default_logger_;
    std::shared_ptr<ThreadPool>default_pool_;

    Manager()
    {
        default_pool_=std::make_shared<ThreadPool>(2,100);
        AsyncLoggerBuilder builder;
        builder.setLoggerName("default");

        Util::JsonUtil::JsonData config_data;
        config_data.loadConfig("./log_config.conf");
        builder.setConfig(config_data);

        default_logger_=builder.build(default_pool_);
    }
public:
    ~Manager()=default;
    Manager& operator =(const Manager&) = delete;
    Manager(const Manager&) = delete;

    static Manager& getInstance()
    {
        static Manager instance;
        return instance;
    }

    std::shared_ptr<AsyncLogger>getDefaultLogger(){return default_logger_;}

    void addLogger(std::shared_ptr<AsyncLogger>logger)
    {
        std::lock_guard<std::mutex>lock(mtx_);
        loggers_[logger->name()] = logger;
    }

    bool exist(std::string name)
    {
        std::lock_guard<std::mutex>lock(mtx_);
        return loggers_.find(name)!=loggers_.end();
    }

    std::shared_ptr<AsyncLogger>getLogger(std::string name)
    {
        std::lock_guard<std::mutex>lock(mtx_);
        if(loggers_.find(name)!=loggers_.end())
        {
            return loggers_[name];
        }
        return nullptr;
    }
    
};

} // namespace asynclog
