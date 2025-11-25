#pragma once

#include <string>
#include <vector>
#include <cstdarg>
#include <memory>

#include "LogFlush.hpp"
#include "AsyncWorker.hpp"
#include "backlog/CliBackUpLog.hpp"
#include "ThreadPool.hpp"
#include "Message.hpp"
#include "Level.hpp"
#include "ISystemOps.h"

namespace asynclog
{
class RealSystemStrOps: public ISystemStrOps
{
public:
    ~RealSystemStrOps() = default;

    int vasprintf(char **ret,const char *fmt,va_list ap)override{return ::vasprintf(ret,fmt,ap);}
};

class AsyncLogger
{
protected:
    std::string logger_name_;
    std::vector<std::shared_ptr<LogFlush>>flushes_; //将日志刷新到多个地方
    std::unique_ptr<AsyncWorker> worker_;
    std::shared_ptr<ThreadPool>thread_pool_;
    std::unique_ptr<ISystemStrOps>ops_;
    Util::JsonUtil::JsonData config_data_;
    size_t max_buffer_size_;

    void serialize(LogLevel::value level,const std::string& file,size_t line,char *ret)
    {
        //构建消息体
        LogMessage message(level,line,file,logger_name_,ret);
        std::string data=message.format();

        if(level==LogLevel::value::ERROR||level==LogLevel::value::FATAL)
        {
            try
            {
                auto ret=thread_pool_->enqueue(start_backup,data,
                    config_data_.backup_addr_,config_data_.backup_port_);  
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            
        }
        flush(data.c_str(),data.size());
    }

    void flush(const char* data,size_t len)
    {
        //因为AsyncWorker是线程安全的，所以此处不用加锁
        worker_->push(data,len);
    }

    //将缓冲区中的数据刷新到磁盘中
    void realFlush(Buffer&buf)
    {   
        for(auto&f:flushes_)
        {
            f->flush(buf.peek(),buf.readableBytes());
        }
        buf.moveReadPos(buf.readableBytes());
    }
public:
    AsyncLogger(std::string logger_name,const std::vector<std::shared_ptr<LogFlush>>&flushes
        ,std::shared_ptr<ThreadPool>pool,Util::JsonUtil::JsonData config_data
        ,BufferPolicy buf_policy=BufferPolicy::UNLIMITED,size_t max_buffer_size=16*1024
        ,std::unique_ptr<ISystemStrOps>ops=nullptr)
        :logger_name_(std::move(logger_name))
        ,flushes_(flushes)
        ,thread_pool_(pool)
        ,config_data_(std::move(config_data))
    {
        if(ops)
        {
            ops_=std::move(ops);
        }
        else
        {
            ops_=std::make_unique<RealSystemStrOps>();
        }
        //这里不要在初始化列表中构造AsyncWorker，因为config_data_使用了move，不管用哪个变量都可能是空的
        worker_=std::make_unique<AsyncWorker>(config_data_,[this](Buffer&buf){realFlush(buf);},buf_policy,max_buffer_size);
        worker_->start();
    }
    ~AsyncLogger()
    {

    }

    bool debug(const std::string&file,size_t line,const std::string&format,...)
    {
        //获取可变参数列表
        va_list args;
        va_start(args,format);

        char * ret;
        int r = ops_->vasprintf(&ret,format.c_str(),args);
        va_end(args);

        if(r==-1||ret==nullptr)
        {
            return false;
        }

        serialize(LogLevel::value::DEBUG,file,line,ret);

        free(ret);
        return true;
    }

    bool info(const std::string&file,size_t line,const std::string&format,...)
    {
        //获取可变参数列表
        va_list args;
        va_start(args,format);

        char * ret;
        int r = ops_->vasprintf(&ret,format.c_str(),args);
        va_end(args);

        if(r==-1||ret==nullptr)
        {
            return false;
        }

        serialize(LogLevel::value::INFO,file,line,ret);

        free(ret);
        return true;
    }

    bool warn(const std::string&file,size_t line,const std::string&format,...)
    {
        //获取可变参数列表
        va_list args;
        va_start(args,format);

        char * ret;
        int r = ops_->vasprintf(&ret,format.c_str(),args);
        va_end(args);

        if(r==-1||ret==nullptr)
        {
            return false;
        }

        serialize(LogLevel::value::WARN,file,line,ret);

        free(ret);
        return true;
    }

    bool error(const std::string&file,size_t line,const std::string&format,...)
    {
        //获取可变参数列表
        va_list args;
        va_start(args,format);

        char * ret;
        int r = ops_->vasprintf(&ret,format.c_str(),args);
        va_end(args);

        if(r==-1||ret==nullptr)
        {
            return false;
        }

        serialize(LogLevel::value::ERROR,file,line,ret);

        free(ret);
        return true;
    }

    bool fatal(const std::string&file,size_t line,const std::string&format,...)
    {
        //获取可变参数列表
        va_list args;
        va_start(args,format);

        char * ret;
        int r = ops_->vasprintf(&ret,format.c_str(),args);
        va_end(args);

        if(r==-1||ret==nullptr)
        {
            return false;
        }

        serialize(LogLevel::value::FATAL,file,line,ret);

        free(ret);
        return true;
    }
};

class AsyncLoggerBuilder
{
protected:
    BufferPolicy buffer_policy_;
    std::string logger_name_;
    std::vector<std::shared_ptr<LogFlush>>flushes_;
    size_t max_buffer_size_;
    Util::JsonUtil::JsonData config_data_;
public:
    AsyncLoggerBuilder(/* args */)
        :buffer_policy_(BufferPolicy::UNLIMITED)
        ,logger_name_("async_logger")
    {
    }
    ~AsyncLoggerBuilder()
    {
    }
    void setLoggerName(std::string name){logger_name_=std::move(name);}
    void setConfig(Util::JsonUtil::JsonData json_data){config_data_=json_data;}
    void setBufferPolicy(BufferPolicy policy){buffer_policy_=policy;}
    void setMaxBufferSize(size_t size){max_buffer_size_=size;}

    template<typename FlushType,typename... Args>
    void addLogFlush(Args&&... args)
    {
         flushes_.emplace_back(
            LogFlushFactory<FlushType>::createLogFlush(std::forward<Args>(args)...));
    }

    std::shared_ptr<AsyncLogger> build(std::shared_ptr<ThreadPool>pool)
    {
        if(flushes_.empty()) addLogFlush<StdOutFlush>();
        return std::make_shared<AsyncLogger>(logger_name_,flushes_,pool,config_data_,buffer_policy_,max_buffer_size_);
    }

};

}//namespace asynclog
