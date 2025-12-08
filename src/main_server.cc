#include <iostream>
#include <signal.h>
#include "server/Service.hpp"


int main()
{
    //忽略sigpipe信号
    signal(SIGPIPE,SIG_IGN);
    //初始化日志器
    mystorage::initServerLog();
    mystorage::Config config;
    config.readConfig("./Storage.conf");
    mystorage::Service server(config);
    server.start();
}