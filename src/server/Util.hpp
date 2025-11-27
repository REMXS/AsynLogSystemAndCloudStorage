#pragma once

#include <jsoncpp/json/json.h>
#include <sstream>
#include <memory>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>

#include "bundle.h"
#include "ServerLog.hpp"

namespace mystorage
{
namespace fs=std::filesystem;


//在网络编程中，处理的基本单位是字节，所以这里的数字也使用unsigned char来表示
inline unsigned char toHex(unsigned char number)
{
    return number>9?number+55:number+48;
}

inline unsigned char fromHex(unsigned char hex_char)
{
    if(hex_char>='0'&&hex_char<='9')
    {
        return hex_char-'0';
    }
    else if(hex_char>='a'&&hex_char<='e')
    {
        return hex_char-'a'+10;
    }
    else if(hex_char>='A'&&hex_char<='E')
    {
        return hex_char-'A'+10;
    }
    else
    {
        throw std::invalid_argument("FromHex: invalid hex character");
    }
}

inline std::string urlDecode(const std::string&str)
{
    std::string ret="";
    for(int i=0;i<str.size();++i)
    {
        //如果是%开头，则将后面两位字符转换为askii编码的字符
        if(str[i]=='%')
        {
            if(i+2>=str.size())
            {
                throw std::runtime_error("UrlDecode: malformed url encoding");
            }

            unsigned char high=fromHex(str[++i]);
            unsigned char low=fromHex(str[++i]);
            ret+=(char)(high*16+low);
        }
        else
        {
            ret+=str[i];
        }
    }

    return ret;
}

class FileUtil
{
private:
    fs::path file_path_;
public:
    FileUtil(fs::path file_path):file_path_(std::move(file_path)){}

    ~FileUtil()=default;

    //获取文件大小
    int64_t getFileSize()
    {
        std::error_code ec;
        auto size=fs::file_size(file_path_,ec);

        if(ec)
        {
            LogError(getLogger(),"%s get file size failed: %s",file_path_.c_str(),ec.message().c_str());
            return -1;
        }

        return size;
    }

    //获取文件最近一次修改时间
    time_t lastModifyTime()
    {   
        std::error_code ec;
        auto ftime=fs::last_write_time(file_path_,ec);
        if(ec)
        {
            LogWarn(getLogger(),"%s, Get file modify time failed: %s",file_path_.c_str(),ec.message().c_str());
            return -1;
        }
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        return std::chrono::system_clock::to_time_t(sctp);
    }   
    //获取文件最近一次访问时间
    time_t lastAccessTime()
    {
        struct stat st;
        if(stat(file_path_.c_str(),&st)==-1)
        {
            LogWarn(getLogger(),"%s, Get file access time failed: %s",file_path_.c_str(),std::strerror(errno));
            return -1;
        }
        return st.st_atime;
    }

    //从文件POS处获取len长度字符给content
    static bool getPosLen(std::ifstream&ifs,std::string&buf,size_t pos,size_t len)
    {
        if(len<=0||pos<0) return false;

        if(!ifs.is_open())
        {
            LogWarn(getLogger(),"stream is not open");
            return false;
        }

        // 如果之前的读取操作触发了 EOF (文件结束符)，流会被置为 fail 状态。
        // 不调用 clear() 的话，后续的 seekg 会直接失败
        ifs.clear();

        ifs.seekg(pos,std::ios::beg);

        if(!ifs.good())
        {
            LogError(getLogger(),"%s seekg failed",__FUNCTION__);
            return false;
        }

        //读数据
        buf.resize(len);
        ifs.read(buf.data(),len);

        if(ifs.gcount()==0&&len>0)
        {
            LogWarn(getLogger(),"%s read 0 bytes",__FUNCTION__);
            return false;
        }

        //如果读取的文件字节数小于buf的大小，则调整buf的大小
        if(ifs.gcount()<len)
        {
            buf.resize(ifs.gcount());
        }

        return true;
    }


    //获取文件名
    inline std::string getFileName()
    {
        return file_path_.filename().string();
    }

    //获取文件内容
    bool getFileContent(std::string&buf)
    {
        std::ifstream ifs(file_path_,std::ios::binary);
        bool ret=getPosLen(ifs,buf,0,getFileSize());
        ifs.close();
        return ret;
    }

    //写文件
    bool setContent(const char* data,size_t len)
    {
        if(data==nullptr||len==0)
        {
            LogInfo(getLogger(),"setContent: invalid argument");
            return false;
        }
        std::ofstream ofs(file_path_,std::ios::binary);
        if(!ofs.is_open())
        {
            LogInfo(getLogger(),"%s open file failed",file_path_.c_str());
            ofs.close();
            return false;
        }
        ofs.write(data,len);

        
        if(!ofs.good())
        {
            LogInfo(getLogger(),"%s write file content failed",file_path_.c_str());
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }
    //压缩文件

    //目录操作


};

    
} // namespace mystorge
