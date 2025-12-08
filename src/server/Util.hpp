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
#include <zstd.h>


#include "ServerLog.hpp"
#include "IZstdOps.h"

namespace mystorage
{
namespace fs=std::filesystem;


//在网络编程中，处理的基本单位是字节，所以这里的数字也使用unsigned char来表示
//将10进制数字转换为16进制字符
inline unsigned char toHex(unsigned char number)
{
    return number>9?number+55:number+48;
}

//将16进制字符转换为10进制数字
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

class ZstdOps: public IZipdOps
{
public:
    ~ZstdOps()=default;
    bool compress(const std::string& input,std::string&output, int level=3) override
    {
        //计算压缩后的可能大小
        size_t bound = ZSTD_compressBound(input.size());

        try {
            output.resize(bound);
        } catch (const std::bad_alloc& e) {
            // 捕获了异常，将其转化为错误码
            LogError(getLogger(), "Zstd compress memory allocation failed: %s", e.what());
            return false;
        }

        //执行压缩操作
        size_t compress_size=ZSTD_compress(output.data(),bound,input.c_str(),input.size(),level);

        if(ZSTD_isError(compress_size))
        {
            LogError(getLogger(),"compressed error %s",ZSTD_getErrorName(compress_size));
            return false;
        }
        //缩减到实际大小
        output.resize(compress_size);
        return true;
    }
    bool decompress(const std::string& input ,std::string&output) override
    {
        //从压缩数据中解析原始大小
        unsigned long long originalSize = ZSTD_getFrameContentSize(input.c_str(),input.size());
        if(originalSize == ZSTD_CONTENTSIZE_ERROR)
        {
            LogError(getLogger(),"Can't analysis the original data size");
            return false;
        }
        if(originalSize == ZSTD_CONTENTSIZE_UNKNOWN)
        {
            LogError(getLogger(),"Compressed data does not contain original size information");
            return false;
        }

        //预分配空间
        try {
            output.resize(originalSize);
        } catch (const std::bad_alloc& e) {
            // 捕获了异常，将其转化为错误码
            LogError(getLogger(), "Zstd compress memory allocation failed: %s", e.what());
            return false;
        }

        size_t decompress_size=ZSTD_decompress(output.data(),output.size(),input.c_str(),input.size());

        if(ZSTD_isError(decompress_size))
        {
            LogError(getLogger(),"decompressed error %s",ZSTD_getErrorName(decompress_size));
            return false;
        }

        output.resize(decompress_size);
        return true;
    }

    std::string getPostfix() override
    {
        return ".zst";
    }
};

class FileUtil
{
private:
    fs::path file_path_;
    std::unique_ptr<IZipdOps>zip_ops_;
public:
    FileUtil(fs::path file_path,std::unique_ptr<IZipdOps>zip_ops=nullptr)
        :file_path_(std::move(file_path))
    {
        if(zip_ops)
        {
            zip_ops_=std::move(zip_ops);
        }
        else
        {
            zip_ops_=std::make_unique<ZstdOps>();
        }
    }

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
    //压缩内容到文件
    bool compress(const std::string& content,int level)
    {
        std::string packed;
        bool is_packed=zip_ops_->compress(content,packed,level);
        if(!is_packed)
        {
            LogInfo(getLogger(),"Compress error");
            return false;
        }
        //将压缩内容写入文件
        if(!setContent(packed.c_str(),packed.size()))
        {
            LogInfo(getLogger(),"filename:%s, Compress SetContent error",file_path_.c_str());
            return false;
        }
        return true;
    }

    //解压内容从文件
    bool uncompress(fs::path download_path)
    {
        std::string content;
        if(!getFileContent(content))
        {
            LogInfo(getLogger(),"%s filename: %s get content failed",__FUNCTION__,file_path_.c_str());
            return false;
        }

        std::string unpacked;
        bool is_unpacked=zip_ops_->decompress(content,unpacked);
        if(!is_unpacked)
        {
            LogInfo(getLogger(),"Decompress error");
            return false;
        }
        FileUtil fu(download_path);
        if(!fu.setContent(unpacked.c_str(),unpacked.size()))
        {
            LogInfo(getLogger(),"%s filename: %s set content failed",__FUNCTION__,download_path.c_str());
            return false;
        }
        return true;
    }


    //目录操作

    bool exists()
    {
        return fs::exists(file_path_);
    }

    bool createDirectory()
    {
        if(fs::is_regular_file(file_path_)) return false;
        if(exists())
        {
            return true;
        }
        std::error_code ec;
        fs::create_directory(file_path_,ec);
        if(ec)
        {
            LogError(getLogger(),"%s create directory failed: %s",file_path_.c_str(),ec.message().c_str());
            return false;
        }
        return true;
    }

    bool scanDirectory(std::vector<fs::path>&arr)
    {
        if(fs::is_regular_file(file_path_)) return false;
        for(auto&p : fs::directory_iterator(file_path_))
        {
            if(fs::is_directory(p)) continue;

            //relative_path 为带有路径的文件名
            arr.emplace_back(fs::path(p).relative_path());
        }
        return true;
    }

};

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

} // namespace JsonUtil
    
} // namespace mystorge
