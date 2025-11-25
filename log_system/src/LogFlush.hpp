#pragma once

#include <memory>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <concepts>

#include "Util.hpp"
#include "ISystemOps.h"

namespace asynclog
{

class RSystemOps: public ISystemOps
{
public:
    FILE* fopen(const char* filename, const char* mode)override{return ::fopen(filename,mode);}
    int fclose(FILE* stream)override{return ::fclose(stream);}
    size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream)override{return ::fwrite(ptr,size,nmemb,stream);}
    int fflush(FILE* stream)override{return ::fflush(stream);}
    int fileno(FILE* stream)override{return ::fileno(stream);}
    int fsync(int fd)override{return ::fsync(fd);}
    int ferror(FILE* stream)override{return ::ferror(stream);}
    
    void perror(const char* s)override{return ::perror(s);}

    void createDirectory(const std::string& path) override { 
        Util::File::createDirectory(path); 
    }
    time_t now() override { 
        return Util::Date::now(); 
    }
};

class LogFlush
{
public:
    using ptr=std::shared_ptr<LogFlush>;
    virtual void flush(const char* data,size_t len) = 0;
    virtual ~LogFlush()=default;
};

class StdOutFlush: public LogFlush
{
public:
    void flush(const char* data,size_t len) override
    {
        std::cout.write(data,len);
    }
    ~StdOutFlush()override =default;
};

class FileFlush: public LogFlush
{
private:
    std::string file_path_;
    FILE* file_;
    size_t flush_log_;
    std::unique_ptr<ISystemOps>ops_;
public:
    FileFlush(std::string file_path,const Util::JsonUtil::JsonData&json_data,std::unique_ptr<ISystemOps>ops=nullptr)
        :file_path_(file_path)
        ,flush_log_(json_data.flush_log_)
        ,file_(NULL)
    {
        if(ops)
        {
            ops_=std::move(ops);
        }
        else
        {
            ops_=std::make_unique<RSystemOps>();
        }

        //创建目录
        ops_->createDirectory(Util::File::folderPath(file_path));
        //打开文件
        file_=ops_->fopen(file_path.c_str(),"ab");
        if(file_==NULL)
        {
            ops_->perror("ops_->fopen failed: ");
        }
    }
    ~FileFlush()override
    {
        if(file_) ops_->fclose(file_);
    }

    void flush(const char* data,size_t len) override
    {
        const char* data_ptr=data;
        size_t remaining=len;
        while(remaining>0)
        {
            size_t n = ops_->fwrite(data_ptr,1,remaining,file_);
            if(n==0)
            {
                if(ops_->ferror(file_))
                {
                    ops_->perror("ops_->fwrite failed: ");
                    return;
                }
                break;
            }
            data_ptr+=n;
            remaining-=n;
        }

        //如果flush_log_是1，则将日志从用户缓冲区刷新到内核缓冲区
        if(flush_log_==1||flush_log_==2)
        {
            if(ops_->fflush(file_)==EOF)
            {
                ops_->perror("ops_->fflush failed: ");
                return;
            }
            //如果flush_log_为2，则进一步将日志刷新到磁盘中
            if(flush_log_==2)
            {
                if(ops_->fsync(ops_->fileno(file_))!=0)
                {
                    ops_->perror("ops_->fsync failed: ");
                }
            }
        }
    }
};

class RollFileFlush: public LogFlush
{
private:
    size_t cnt_;            //滚动文件的序号
    size_t cur_cnt_;        //当前文件写入的大小
    size_t max_size_;       //每个日志文件允许的最大的大小
    FILE* file_;
    std::string folder_path_;//存放日志文件的目录
    size_t flush_log_;      //日志的刷新策略
    std::unique_ptr<ISystemOps>ops_;

    //初始化日志文件
    void initLogFile()
    {
        //如果没有文件或者是当前文件达到最大，创建一个新文件
        if(file_== NULL || cur_cnt_>max_size_)
        {
            if(file_ != NULL)
            {
                ops_->fclose(file_);
                file_ = NULL;
            }

            //创建日志的路径
            std::string file_path_ =folder_path_+createFileName();
            
            file_=ops_->fopen(file_path_.c_str(),"ab");
            if(file_ == NULL)
            {
                throw std::runtime_error("new file open failed "+file_path_);
            }
            cur_cnt_=0;
        }
    }

    //创建日志文件的名字
    std::string createFileName()
    {
        time_t t = ops_->now();
        struct tm tm;
        
        ::localtime_r(&t,&tm);

        std::stringstream ss;
        ss<<std::put_time(&tm,"LOG_%Y-%m-%d_%H:%M%S");
        ss<<"-"<<cnt_++<<".log";
        return ss.str();
    }
public:
    RollFileFlush(const std::string& folder_path,size_t per_file_max_size,
        const Util::JsonUtil::JsonData&json_data,std::unique_ptr<ISystemOps>ops=nullptr)
        :cnt_(1)
        ,cur_cnt_(0)
        ,max_size_(per_file_max_size)
        ,file_(NULL)
        ,folder_path_(folder_path)
        ,flush_log_(json_data.flush_log_)
    {
        if(ops)
        {
            ops_=std::move(ops);
        }
        else
        {
            ops_=std::make_unique<RSystemOps>();
        }

        ops_->createDirectory(folder_path_);
        std::string opt;
        //规范文件的路径
        #if defined(_WIN32)
        opt = r"(//)";
        #else
        opt = "/";
        #endif
        if(folder_path_.back()!='/'&&folder_path_.back()!='\\')
        {
            folder_path_+=opt;
        }
    }
    ~RollFileFlush()
    {
        if(file_ != NULL)
        {
            ops_->fclose(file_);
        }
    }

    //测试使用
    inline size_t getFileNum()const {return cnt_-1;}

    void flush(const char* data,size_t len)override
    {
        initLogFile();

        const char* data_ptr=data;
        size_t remaining=len;
        while(remaining>0)
        {
            size_t n = ops_->fwrite(data_ptr,1,remaining,file_);
            if(n==0)
            {
                if(ops_->ferror(file_))
                {
                    ops_->perror("ops_->fwrite failed: ");
                    return;
                }
                break;
            }
            data_ptr+=n;
            remaining-=n;
        }
        cur_cnt_+=len;

        //如果flush_log_是1，则将日志从用户缓冲区刷新到内核缓冲区
        if(flush_log_==1||flush_log_==2)
        {
            if(ops_->fflush(file_)==EOF)
            {
                ops_->perror("ops_->fflush failed: ");
                return;
            }
            //如果flush_log_为2，则进一步将日志刷新到磁盘中
            if(flush_log_==2)
            {
                if(ops_->fsync(ops_->fileno(file_))!=0)
                {
                    ops_->perror("ops_->fsync failed: ");
                }
            }
        }
    }
};


template<typename T>
concept isFlush = std::is_base_of_v<LogFlush,T>;

template<isFlush FlushType_>
class LogFlushFactory
{
public:
    template<typename ... Args>
    static std::shared_ptr<LogFlush> createLogFlush(Args &&... args)
    {
        return std::make_shared<FlushType_>(std::forward<Args>(args)...);
    }   
};


} // namespace asynclog
