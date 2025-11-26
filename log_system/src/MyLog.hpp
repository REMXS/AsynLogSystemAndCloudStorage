#pragma once

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