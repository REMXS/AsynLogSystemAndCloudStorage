#pragma once

#include <memory>
#include <thread>
#include <sstream>

#include <Util.hpp>
#include <Level.hpp>


namespace asynclog
{
//一条日志的结构信息
struct LogMessage
{
    using ptr = std::shared_ptr<LogMessage>;
    LogMessage() = default;
    LogMessage(LogLevel::value level,size_t line,std::string file,
        std::string name,std::string pay_load)
        :line_(line)
        ,ctime_(Util::Date::now())
        ,file_name_(file)
        ,name_(name)
        ,pay_load_(pay_load)
        ,tid_(std::this_thread::get_id())
        ,level_(level)
    {}

    std::string format()
    {
        struct tm tm_;
        localtime_r(&ctime_,&tm_);
        char buf[128] = {0};
        strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",&tm_);

        std::string tem1 = '['+std::string(buf)+"][";
        std::string tem2 = "]["+ std::string(LogLevel::toString(level_))+"]["+name_+"]["+file_name_+':'+std::to_string(line_)+"]\t"+pay_load_+'\n';

        std::stringstream ret;
        ret<<tem1<<tid_<<tem2;
        return ret.str();
    }
    


    size_t line_; //行号
    time_t ctime_; //时间
    std::string file_name_; //文件名
    std::string name_; //日志器名 相当于一个标签，说明日志来源的模块
    std::string pay_load_; //信息体
    std::thread::id tid_; //线程名
    LogLevel::value level_; //日志等级
};
} // namespace asynclog
