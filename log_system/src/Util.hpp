#pragma once

#include <sys/types.h>
#include <sys/stat.h>

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

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
void createDictory(const std::string&path_name)
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



} // namespace asynclog::Util
