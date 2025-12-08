#pragma once

#include <regex>
#include <string>
#include <memory>
#include <iomanip>
#include <tuple>
#include <fcntl.h>
#include <sstream>

#include <event.h>
#include <evhttp.h>
#include <event2/http.h>
#include <event2/util.h>



#include "DataManager.hpp"
#include "Config.hpp"
#include "ServerLog.hpp"

#include "base64.h"

namespace mystorage
{
class Service
{
private:
    uint16_t port_;     //端口
    std::string ip_;    //ip地址
    Config conf_;       //配置文件
    const std::string temp_download_dir;
    std::shared_ptr<DataManager> data_manager_;

    //利用RAII避免资源泄露
    struct EventBaseDeleter 
    {
        void operator()(struct event_base* base)
        {
            if(base) event_base_free(base);
        }
    };
    using EventBasePtr=std::unique_ptr<event_base,EventBaseDeleter>;

    struct EvhttpDeleter 
    {
        void operator()(struct evhttp* http)
        {
            if(http) evhttp_free(http);
        }
    };
    using EvhttpPtr=std::unique_ptr<evhttp,EvhttpDeleter>;


    //libevent中http解析完成后通用的处理函数
    void genHandler(struct evhttp_request*req,void* args)
    {
        //获取路径
        std::string path = evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));
        //根据请求中的内容判断是什么请求
        //下载请求
        if(path.find("/download/")!=std::string::npos)
        {
            downLoad(req,args);
        }
        //上传请求
        else if(path.find("upload")!=std::string::npos)
        {
            upload(req,args);
        }
        //显示已存储的文件列表
        else if(path=="/")
        {
            listShow(req,args);
        }
        //资源不存在，返回404
        else
        {
            evhttp_send_reply(req,HTTP_NOTFOUND,"not found",nullptr);
        }
    }


    static void genHandlerAdapter(struct evhttp_request*req,void* args)
    {
        auto* self = static_cast<Service*>(args);
        self->genHandler(req,nullptr);
    }

    //下载文件业务处理函数 
    void downLoad(struct evhttp_request*req,void* args)
    {
        //获取客户端请求的资源路径
        //从资源路径中获取对应资源的storageinfo
        StorageInfo info;
            //获取路径
        std::string url_path=evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));

        if(!data_manager_->getOneByURL(url_path,info))
        {
            LogInfo(getLogger(),"the file information corresponding to the url doesn't exist");
            evhttp_send_reply(req,HTTP_BADREQUEST,"the file information corresponding to the url doesn't exist",nullptr);
            return;
        }

        LogInfo(getLogger(),"request url_path:%s", url_path.c_str());
        //如果压缩过就解压到新文件给用户下载
        std::string download_path;
        if(info.storage_path_.find(conf_.getDeepStorageDir())!=std::string::npos)
        {
            LogInfo(getLogger(),"uncompressing:%s", info.storage_path_.c_str());
            FileUtil fu(info.storage_path_);
            download_path=temp_download_dir+fu.getFileName();
            
            //temp_download_dir 的创建在构造函数中已经完成了
            fu.uncompress(download_path);
        }
        else
        {
            download_path=info.storage_path_;
        }

        //检查文件是否存在
        FileUtil fu(download_path);
        if(fu.exists()==false)
        {
            if(info.storage_path_.find(conf_.getDeepStorageDir())!=std::string::npos)
            {
                LogError(getLogger(),"file %s decompress failed",info.storage_path_.c_str());
            }
            else
            {
                LogError(getLogger(),"file %s doesn't exist",download_path.c_str());
            }
            evhttp_send_reply(req,HTTP_BADREQUEST,nullptr,nullptr);
        }

        //确定文件是否需要断点续传
        size_t file_size=fu.getFileSize();
        size_t range_start=0;
        size_t range_end=file_size-1;
        bool is_range_request=false;

        //解析range请求头
        auto range_header=evhttp_find_header(req->input_headers,"Range");
        if(range_header!=nullptr)
        {
            auto[success,start,end]=httpParseRange(range_header);
            is_range_request=success;
            if(is_range_request)
            {
                range_start=start;
                //end如果没有会返回0
                if(end) range_end=end;
            }
        }

        //检查if—range
        auto if_range_header=evhttp_find_header(req->input_headers,"If-Range");
        if(if_range_header!=nullptr&&is_range_request)
        {
            std::string curr_etag=getEtag(info);
            if(std::string(if_range_header)!=curr_etag)
            {
                //文件已变化，忽略range，发送整个文件
                is_range_request=false;
                range_start=0;
                range_end=file_size-1;
                LogInfo(getLogger(),"%s target file has been changed!",info.url_);
            }
        }
        //验证范围合法性
        if(range_start>=file_size)
        {
            //如果不合法，则直接发送错误码416
            evhttp_add_header(req->output_headers,"Content-Range",("bytes */"+std::to_string(file_size)).c_str());
            evhttp_send_reply(req,416,"Range Not Satisfiable",nullptr);
            return;
        }
        range_end=std::min(range_end,file_size-1);

        size_t content_length= range_end-range_start+1;

        //将文件读入responce body中
        int fd=open(download_path.c_str(),O_RDONLY);
        if(fd==-1)
        {
            LogError(getLogger(),"%s open file failed errno: %s",download_path.c_str(),strerror(errno));
            evhttp_send_reply(req,HTTP_INTERNAL,"server error",nullptr);
            return;
        }

        evbuffer* output_buf=evhttp_request_get_output_buffer(req);
        if(evbuffer_add_file(output_buf,fd,range_start,content_length)==-1)
        {
            LogError(getLogger(),"%s evbuffer_add_file failed",download_path.c_str());
            evhttp_send_reply(req,HTTP_INTERNAL,"server error",nullptr);
            return;
        }

        //设置响应头
        std::stringstream content_disposition;
        content_disposition<<"attachment; filename=\""<<fu.getFileName()<<"\"";
        evhttp_add_header(req->output_headers,"Accept-Ranges","bytes");
        evhttp_add_header(req->output_headers,"ETag",getEtag(info).c_str());
        evhttp_add_header(req->output_headers,"Content-Range","application/octet-stream");
        evhttp_add_header(req->output_headers,"Content-Disposition",content_disposition.str().c_str());
        //发送响应
        if(is_range_request)
        {
            std::stringstream content_range;
            content_range<<"bytes "<<std::to_string(range_start)<<"-"<<std::to_string(range_end)<<"/"<<std::to_string(file_size);
            evhttp_add_header(req->output_headers,"Content-Range",content_range.str().c_str());   
            evhttp_send_reply(req,206,"Partial Content",nullptr);
            LogInfo(getLogger(),"206 Partial Content: %s", content_range.str().c_str());
        }
        else
        {
            evhttp_send_reply(req,HTTP_OK,nullptr,nullptr);
            LogInfo(getLogger(),"200 full file");
        }
        //如果是解压的资源，清除临时资源
        if(download_path!=info.storage_path_)
        {
            fs::remove(download_path);
        }
    }


    //上传文件业务处理函数
    void upload(struct evhttp_request*req,void* args)
    {
        LogDebug(getLogger(),"start upload");
        //获取请求体
        evbuffer* buf=evhttp_request_get_input_buffer(req);
        if(!buf)
        {
            LogInfo(getLogger(),"%s evbuffer is empty",__FUNCTION__);
            evhttp_send_reply(req,HTTP_BADREQUEST,"input buffer is empty",nullptr);
            return;
        }

        size_t len=evbuffer_get_length(buf);
        if(len==0)
        {
            evhttp_send_reply(req, HTTP_BADREQUEST, "file empty", nullptr);
            LogInfo(getLogger(),"%s request body is empty",__FUNCTION__);
            return;
        }

        std::string content(len,0);

        if(evbuffer_copyout(buf,content.data(),len)==-1)
        {
            LogError(getLogger(),"evbuffer_copyout failed");
            evhttp_send_reply(req, HTTP_INTERNAL, "file empty", nullptr);
            return;
        }
        //获取文件名
        std::string file_name = evhttp_find_header(req->input_headers,"FileName");
        file_name = base64_decode(file_name);

        //获取存储类型
        std::string storage_type = evhttp_find_header(req->input_headers,"StorageType");
        std::string storage_path;
        if(storage_type=="low")
        {
            storage_path= conf_.getLowStorageDir();
        }
        else if(storage_type=="deep")
        {
            storage_path= conf_.getDeepStorageDir();
        }
        else
        {
            LogInfo(getLogger(),"invaild storage_type");
            evhttp_send_reply(req, HTTP_BADREQUEST, "invaild storage_type", nullptr);
            return;
        }
        FileUtil tar_dir(storage_path);
        tar_dir.createDirectory();

        //拼接成完整的文件路径
        storage_path+=file_name;

        //根据存储类型检查是否要压缩文件
        FileUtil fu(storage_path);
        if(storage_path.find(conf_.getDeepStorageDir())!=std::string::npos)
        {
            if(!fu.compress(content,3))
            {
                LogError(getLogger(),"deep storage error! file path: %s",storage_path.c_str());
                evhttp_send_reply(req, HTTP_INTERNAL, "server error", nullptr);
                return;
            }
        }
        else
        {
            if(!fu.setContent(content.c_str(),content.size()))
            {
                LogError(getLogger(),"low storage error! file path: %s",storage_path.c_str());
                evhttp_send_reply(req, HTTP_INTERNAL, "server error", nullptr);
                return;
            }
        }

        //存储文件条目
        StorageInfo info(conf_);
        info.newStorageInfo(storage_path);
        data_manager_->insert(info);
        
        evhttp_send_reply(req, HTTP_OK, "Success", nullptr);
        LogInfo(getLogger(),"upload success file path: %s",storage_path.c_str());
    }


    //获取文件列表
    void listShow(struct evhttp_request*req,void* args)
    {
        LogDebug(getLogger(),"start to show list");
        //读取所有的文件存储信息
        std::vector<StorageInfo>infos;
        if(!data_manager_->getAll(infos))
        {
            LogError(getLogger(),"read storage info failed!");
            evhttp_send_reply(req,HTTP_INTERNAL,"server error",nullptr);
            return;
        }

        //读取页面的模板信息
        std::ifstream template_file("index.html",std::ios::binary|std::ios::ate);
        if(!template_file.is_open())
        {
            LogError(getLogger(),"index.html open failed!");
            evhttp_send_reply(req,HTTP_INTERNAL,"server error",nullptr);
            return;
        }

        size_t file_size= template_file.tellg();
        template_file.seekg(0,std::fstream::beg);
            //读取文件
        std::string template_file_content(file_size,0);
        template_file.read(template_file_content.data(),file_size);

        //用正则表达式替换内容
        template_file_content=
            std::regex_replace(template_file_content,std::regex(R"(\{\{FILE_LIST\}\})"),generateModernFileList(infos));
        template_file_content=
            std::regex_replace(template_file_content,std::regex(R"(\{\{BACKEND_URL\}\})"),conf_.getServerIp()+':'+std::to_string(conf_.getServerPort()));
        //构建http响应报文
        evbuffer* output_buf=evhttp_request_get_output_buffer(req);
        evbuffer_add(output_buf,template_file_content.c_str(),template_file_content.size());
        evhttp_add_header(req->output_headers,"Content-type","text/html;charset=utf-8");
        evhttp_send_reply(req,HTTP_OK,nullptr,nullptr);
        LogDebug(getLogger(),"show list successfully");
    }

    //将time_t类型的事件转换为字符串
    inline static std::string timeToStr(const time_t t)
    {
        return ctime(&t);
    }

    //将文件大小格式化为适合人类阅读的格式
    static std::string formatSize(uint64_t file_size)
    {
        const char* units[]={"B","KB","MB","GB"};
        double size=file_size;

        size_t units_idx=0;

        while(size>=1024&&units_idx<3)
        {
            size/=1024;
            units_idx++;
        }
        std::stringstream ss;
        ss<<std::fixed<<std::setprecision(2)<<size<<units[units_idx];
        return ss.str();
    }

    //获取文件的etag
    static std::string getEtag(const StorageInfo& info)
    {
        FileUtil fu(info.storage_path_);
        std::string etag=fu.getFileName();
        etag+='-';
        etag+=std::to_string(info.file_size_);
        etag+='-';
        etag+=std::to_string(info.mtime_);
        return etag;
    }

    std::string generateModernFileList(const std::vector<StorageInfo> &files)
    {
        std::stringstream ss;
        ss << "<div class='file-list'><h3>已上传文件</h3>";

        for (const auto &file : files)
        {
            std::string filename = FileUtil(file.storage_path_).getFileName();

            // 从路径中解析存储类型（示例逻辑，需根据实际路径规则调整）
            std::string storage_type = "low";
            if (file.storage_path_.find("deep") != std::string::npos)
            {
                storage_type = "deep";
            }

            ss << "<div class='file-item'>"
                << "<div class='file-info'>"
                << "<span>📄" << filename << "</span>"
                << "<span class='file-type'>"
                << (storage_type == "deep" ? "深度存储" : "普通存储")
                << "</span>"
                << "<span>" << formatSize(file.file_size_) << "</span>"
                << "<span>" << timeToStr(file.mtime_) << "</span>"
                << "</div>"
                << "<button onclick=\"window.location='" << file.url_ << "'\">⬇️ 下载</button>"
                << "</div>";
        }

        ss << "</div>";
        return ss.str();
    }

    std::tuple<bool,size_t,size_t> httpParseRange(const std::string&text)
    {
        std::tuple<bool,size_t,size_t>ret=std::make_tuple(false,0,0);
        size_t posi=text.find("bytes=");
        if(posi!=std::string::npos)
        {
            //获取数字部分
            std::string range_str=text.substr(posi+6);
            
            size_t dash_pos=range_str.find('-');
            if(dash_pos!=std::string::npos)
            {
                //解析起始位置
                get<1>(ret)=std::stoull(range_str.substr(0,dash_pos));
                //解析结束位置(如果有)
                if(dash_pos+1<range_str.size())
                {
                    get<2>(ret)=std::stoull(range_str.substr(dash_pos+1));
                }
                get<0>(ret)=true;
            }
        }
        return ret;
    }
    


public:
    Service(Config config_data,std::shared_ptr<DataManager>data_manager=nullptr)
        :conf_(std::move(config_data))
        ,temp_download_dir("./temp_download/")
    {
        port_=conf_.getServerPort();
        ip_=conf_.getServerIp();
        if(data_manager)
        {
            data_manager_=data_manager;
        }
        else
        {
            data_manager_=std::make_shared<DataManager>(conf_);
        }

        //创建临时存放解压文件的目录
        FileUtil temp_dir(temp_download_dir);
        temp_dir.createDirectory();
    }
    ~Service()=default;

    bool start()
    {
        //初始化服务器
        LogDebug(getLogger(),"start to create event_base");
        EventBasePtr base(event_base_new());
        if(base==nullptr)
        {
            const char* err_msg=evutil_socket_error_to_string(errno);
            LogFatal(getLogger(),"event_base_new error! %s",err_msg);
            return false;
        }

        //创建http服务器
        LogDebug(getLogger(),"start to create evhttp");
        EvhttpPtr http(evhttp_new(base.get()));
        if(base==nullptr)
        {
            const char* err_msg=evutil_socket_error_to_string(errno);
            LogFatal(getLogger(),"evhttp_new error! %s",err_msg);
            return false;
        }

        //绑定地址和端口
        if(evhttp_bind_socket(http.get(),ip_.c_str(),port_)!=0)
        {
            const char* err_msg=evutil_socket_error_to_string(errno);
            LogFatal(getLogger(),"evhttp_bind_socket error! %s",err_msg);
            return false;
        }

        //设置处理http报文的回调函数
        evhttp_set_gencb(http.get(),genHandlerAdapter,this);
        
        if(base)
        {
            LogDebug(getLogger(),"start to create evhttp");

            //开始事件监听循环
            if(event_base_dispatch(base.get())!=0)
            {
                const char* err_msg=evutil_socket_error_to_string(errno);
                LogFatal(getLogger(),"event_base_dispatch error! %s",err_msg);
                return false;
            }
        }
        return true;
    }
};


} // namespace mystorage

