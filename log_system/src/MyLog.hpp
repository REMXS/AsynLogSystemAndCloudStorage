#pragma once

#include <sstream>
#include <any>
#include "Manager.hpp"

std::shared_ptr<asynclog::AsyncLogger> DefaultLogger()
{
    return asynclog::Manager::getInstance().getDefaultLogger();
}


#define LogDebug(logger,fmt,...) logger->debug(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogWarn(logger,fmt,...) logger->warn(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogInfo(logger,fmt,...) logger->info(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogError(logger,fmt,...) logger->error(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogFatal(logger,fmt,...) logger->fatal(__FILE__,__LINE__,fmt,##__VA_ARGS__)

#define LogDefaultDebug(fmt,...) DefaultLogger->debug(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogDefaultWarn(fmt,...) DefaultLogger->warn(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogDefaultInfo(fmt,...) DefaultLogger->info(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogDefaultError(fmt,...) DefaultLogger->error(__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define LogDefaultFatal(fmt,...) DefaultLogger->fatal(__FILE__,__LINE__,fmt,##__VA_ARGS__)

enum class LogLevel {
    DEBUG = 1,
    INFO,
    WARN,
    ERROR,
    FATAL
};

struct LOG
{
    std::stringstream ss_;
    std::shared_ptr<asynclog::AsyncLogger>logger_;
    LogLevel level_;
    const char* file_;
    size_t line_;

    LOG(std::shared_ptr<asynclog::AsyncLogger>logger,LogLevel level,const char* file, int line)
        :logger_(logger)
        ,level_(level)
        ,file_(file)
        ,line_(line)
    {}
    ~LOG()
    {
        switch (level_)
        {
        case LogLevel::DEBUG:
        {
            logger_->debug(file_,line_,ss_.str());
            break;
        }
        case LogLevel::INFO:
        {
            logger_->info(file_,line_,ss_.str());
            break;
        }
        case LogLevel::WARN:
        {
            logger_->warn(file_,line_,ss_.str());
            break;
        }
        case LogLevel::ERROR:
        {
            logger_->error(file_,line_,ss_.str());
            break;
        }
        case LogLevel::FATAL:
        {
            logger_->fatal(file_,line_,ss_.str());
            break;
        }
        default:
        {
            logger_->info(file_,line_,ss_.str());
            break;
        }
        }
    }

    template<typename T>
    LOG& operator << (T&& msg)
    {
        ss_<<std::forward<T>(msg);
        return *this;
    }

};

#define MYLOG(logger,level) LOG(logger,level,__FILE__,__LINE__)
