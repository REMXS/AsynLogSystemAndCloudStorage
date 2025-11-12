#pragma once

#include <sys/types.h>
#include <sys/stat.h>

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <memory>

#include <jsoncpp/json/json.h>


namespace asynclog::Util
{

namespace Date
{

//返回当前的时间
time_t now(){return time(nullptr);}

} // namespace Date

namespace File
{

//判断这个路径是否存在
bool exists(const std::string&file_name)
{
    struct stat st;
    return (stat(file_name.c_str(),&st)==0);
}
bool exists(std::string_view file_name)
{
    struct stat st;
    std::string temp(file_name);
    return (stat(temp.c_str(),&st)==0);
}

//输出一个文件所在的目录的路径
std::string folderPath(const std::string&file_name)
{
    if(file_name.empty())
    {
        return "";
    }

    int pos=file_name.find_last_of("/\\");//找到最后一个'/'或者是'\'

    if(pos!=std::string::npos)
    {
        return file_name.substr(0,pos+1);
    }

    return "";
}

//递归创建目录
void createDirectory(const std::string&path_name)
{
    if(path_name.empty())
    {
        perror("the path name is empty!");
    }

    if(exists(path_name))
    {
        return;
    }

    size_t index=0;
    size_t pos=0;
    size_t size=path_name.size();
    while(index<size)
    {
        pos=path_name.find_first_of("/\\",index);//find是查找字符串，而find_first_of才是查找字符

        //如果没有找到，说明已经递归到最后一个目录
        if(pos==std::string::npos)
        {
            if(path_name.back()!='.')
            {
                mkdir(path_name.c_str(),0755);
            }
            return;
        }
        else if(pos==index)
        {
            index++;
            continue;
        }

        //std::string_view sub_path(path_name.begin(),path_name.begin()+pos);
        std::string_view segment(path_name.begin()+index,path_name.begin()+pos);

        if(segment=="."||segment=="..")
        {
            index=pos+1;
            continue;
        }

        //如果子目录存在，继续向后递归
        std::string sub_path=path_name.substr(0,pos);
        if(exists(sub_path))
        {
            index=pos+1;
            continue;
        }

        //创建子目录
        mkdir(sub_path.c_str(),0755);
        index=pos+1;
    }
}

int64_t fileSize(const std::string&file_name)
{
    struct stat st;
    int ret=stat(file_name.c_str(),&st);

    if(ret==-1)
    {
        perror("get file size failed");
        return -1;
    }
    return st.st_size;
}

//获取文件内容
bool getFileContent(std::string&content,const std::string&file_name)
{
    std::ifstream ifs(file_name,std::ios::binary|std::ios::ate);//使用ate标志直接将文件指针定位的文件的末尾
    if(!ifs.is_open())
    {
        std::cerr<<"file open failed"<<std::endl;
        return false;
    }

    //获取文件大小
    auto file_len=ifs.tellg();
    if(file_len==-1)
    {
        std::cerr<<"get file size failed"<<std::endl;
        ifs.close();
        return -1;
    }

    //将文件的指针移回文件的开头
    ifs.seekg(std::ifstream::beg);

    //保证string中有充足的空间
    content.resize(file_len);

    ifs.read(content.data(),file_len);
    if(!ifs.good())
    {
        std::cerr<<__FUNCTION__<<":"<<__LINE__<<"read file content failed"<<std::endl;
        ifs.close();
        return false;
    }

    ifs.close();
    return true;
}
    
} // namespace File

namespace JsonUtil
{

//json对象序列化成字符串
bool serialize(const Json::Value& val,std::string& str, bool pretty=false)
{
    /*
    建造者->构建配置（这里使用默认配置）->构建具体的工作流(StreamWriter),而工作流的具体流程是不变的(从json对象转换为字符串)
    但是配置项是有剧烈变化的(StreamWriterBuilder设置的行前缩进，启动注释之类的功能)
    */
    Json::StreamWriterBuilder swb;
    if(!pretty)
    {
        swb["indentation"] = "";
        swb["commentStyle"] = "None";
    }

    std::unique_ptr<Json::StreamWriter> cwb(swb.newStreamWriter());
    std::stringstream oss;
    if(cwb->write(val,&oss)!=0)
    {
        std::cerr<<__FUNCTION__<<":"<<__LINE__<<" serialize failed"<<std::endl;
        return false;
    }
    str=oss.str();
    return true;
}

//字符串序列化为json对象
bool parse(const std::string& str,Json::Value& val)
{
    Json::CharReaderBuilder crb;
    std::unique_ptr<Json::CharReader>ccr(crb.newCharReader());

    std::string err;
    if(ccr->parse(str.c_str(),str.c_str()+str.size(),&val,&err)==false)
    {
        std::cerr<<__FUNCTION__<<":"<<__LINE__<<" parse failed -> "<<err<<std::endl;
        return false;
    }
    return true;
}


/*
因为这种Meyers 单例实现难以测试，所以把这个类的职责 “1.加载配置 2.保证唯一性”
中的第二项由这个类保证变为由应用程序保证，实现这两个职责的解耦。
struct JsonData
{
    static JsonData& getJsonData()
    {
        static JsonData instance;
        return instance;
    }
    JsonData(const JsonData&)=delete;
    JsonData& operator=(const JsonData&)=delete;

    size_t buffer_size_; //缓冲区的初始大小
    size_t threshold_; //缓冲区的倍数扩容阈值
    size_t linear_growth_; //缓冲区的线性扩容的扩容容量
    size_t flush_log_; //控制日志同步到磁盘的时机，默认为0，1为调用fflush，2为调用fsync
    std::string backup_addr_; //备份的服务器的ip地址
    uint16_t backup_port_; //备份服务器的端口号
    size_t thread_count_; //日志系统内部线程池的数量

private:
    // 加载默认参数的辅助函数
    void loadDefaults() {
        std::cout << "WARNING: config.conf not found or invalid. Using default settings." << std::endl;
        buffer_size_ = 4 * 1024 * 1024; // 4MB
        threshold_ = 1024;              // 1KB
        linear_growth_ = 1024 * 1024;   // 1MB
        flush_log_ = 1;                 // fflush
        backup_addr_ = "127.0.0.1";
        backup_port_ = 8080;
        thread_count_ = 1;
    }

    JsonData()
    {
        std::string content;
        //获取文件失败，使用默认配置
        if(File::getFileContent(content,"log_config.conf")==false)
        {
            loadDefaults();
            return;
        }

        Json::Value root;
        //解析文件失败，使用默认配置
        if(parse(content,root)==false)
        {
            loadDefaults();
            return;
        }

        buffer_size_=root["buffer_size"].asUInt64();
        threshold_=root["threshold"].asUInt64();
        linear_growth_=root["linear_growth"].asUInt64();
        flush_log_=root["flush_log"].asUInt64();
        backup_addr_=root["backup_addr"].asString();
        backup_port_=root["backup_port"].asUInt();
        thread_count_=root["thread_count"].asUInt64();
    }
};
*/

struct JsonData
{
    JsonData(const JsonData&)=delete;
    JsonData& operator=(const JsonData&)=delete;

    size_t buffer_size_; //缓冲区的初始大小
    size_t threshold_; //缓冲区的倍数扩容阈值
    size_t linear_growth_; //缓冲区的线性扩容的扩容容量
    size_t flush_log_; //控制日志同步到磁盘的时机，默认为0，1为调用fflush，2为调用fsync
    std::string backup_addr_; //备份的服务器的ip地址
    uint16_t backup_port_; //备份服务器的端口号
    size_t thread_count_; //日志系统内部线程池的数量

    JsonData()
        :buffer_size_ ( 4 * 1024 * 1024) // 4MB
        ,threshold_  (1024)              // 1KB
        ,linear_growth_ ( 1024 * 1024)   // 1MB
        ,flush_log_ (1)                 // fflush
        ,backup_addr_ ( "127.0.0.1")
        ,backup_port_ (8080)
        ,thread_count_ (1)
    {}

    void loadConfig(const std::string&file_path)
    {
        std::string content;
        //获取文件失败，使用默认配置
        if(File::getFileContent(content,file_path)==false)
        {
            std::cout <<__FUNCTION__<<":"<<__LINE__<<"->WARNING: config.conf not found or invalid. Using default settings." << std::endl;    
            return;
        }

        Json::Value root;
        //解析文件失败，使用默认配置
        if(parse(content,root)==false)
        {
            std::cout <<__FUNCTION__<<":"<<__LINE__<<"->WARNING: parse file failed. Using default settings." << std::endl;    
            return;
        }

        buffer_size_=root["buffer_size"].asUInt64();
        threshold_=root["threshold"].asUInt64();
        linear_growth_=root["linear_growth"].asUInt64();
        flush_log_=root["flush_log"].asUInt64();
        backup_addr_=root["backup_addr"].asString();
        backup_port_=root["backup_port"].asUInt();
        thread_count_=root["thread_count"].asUInt64();
    }
};

} //namespace JsonUtil

} // namespace asynclog::Util
