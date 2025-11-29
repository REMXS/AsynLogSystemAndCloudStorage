#pragma once

#include <unordered_map>
#include <thread>
#include <string>
#include <filesystem>
#include <shared_mutex>
#include <sqlite3.h>

#include "Config.hpp"
#include "ServerLog.hpp"


namespace mystorage
{
namespace fs=std::filesystem;
struct StorageInfo
{
    time_t atime_;              //文件访问时间
    time_t mtime_;              //文件修改时间
    size_t file_size_;          //文件大小
    std::string storage_path_;  //文件实际存储的路径
    std::string url_;           //访问文件的url
    std::string download_prefix_; //下载url 前缀

    StorageInfo()=default;
    StorageInfo(const Config& conf)   
        :download_prefix_(conf.getDownloadPrefix())
    {}

    ~StorageInfo()=default;

    //初始化备份文件的信息
    bool newStorageInfo(fs::path file_path)
    {
        if(download_prefix_.empty())
        {
            LogError(getLogger(),"%s download_prefix_ is empty!");
            return false;
        }
        FileUtil fu(file_path);
        if(!fu.exists())
        {
            LogInfo(getLogger(),"%s file doesn't exist",file_path.c_str());
            return false;
        }
        
        atime_=fu.lastAccessTime();
        mtime_=fu.lastModifyTime();
        file_size_=fu.getFileSize();
        storage_path_=file_path.string();
        url_=download_prefix_+fu.getFileName();

        LogInfo(getLogger(),"download_url:%s,mtime_:%s,atime_:%s,file_size_:%d", url_.c_str(),ctime(&mtime_),ctime(&atime_),file_size_);
        return true;
    }
};

class DataManager
{
private:
    std::shared_mutex mtx_;            //读写锁
    sqlite3 *db_;

    bool execSql(const std::string& sql)
    {
        char* err_msg=nullptr;
        int rc=sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&err_msg);
        if(rc!=SQLITE_OK)
        {
            LogError(getLogger(),"%s execute error! message: %s",err_msg);
            sqlite3_free(err_msg);
            return false;
        }
        return true;
    }
public:
    DataManager(const Config&config_file)
    {
        //从配置文件中获取数据库文件的位置
        std::string db_file=config_file.getStorageInfoFile();
        FileUtil fu(db_file);
        if(!fu.exists())
        {
            throw std::invalid_argument("db file doesn't exist!");
        }
        //打开数据库文件
        int rc=sqlite3_open(db_file.c_str(),&db_);

        //如果打开不成功，直接抛出错误
        if(rc!=SQLITE_OK)
        {
            throw std::runtime_error("open db file failed!");
        }

        //建表，建索引
        const char* createTableSQL=
        R"(create table if not exists tem_table
        (
            url varchar(512) primary key,
            atime bigint,
            mtime bigint,
            storage_path varchar(512) unique,
            file_size bigint
        );
        create index if not exists storage_path_idx on tem_table(storage_path);
        )";

        if(!execSql(createTableSQL))
        {
            throw std::runtime_error("create table failed!");
        }
    }
    ~DataManager()
    {
        if(db_)
        {
            sqlite3_close(db_);
        }
        db_=nullptr;
    }

    bool insert(const StorageInfo &info)
    {
        std::lock_guard<std::shared_mutex>lock(mtx_);
        //准备sql语句
        const char * insert_sql=
            "insert or replace into tem_table(url, atime, mtime, storage_path, file_size) values (?,?,?,?,?);";    
        LogInfo(getLogger(),"%s:%s",__FUNCTION__,insert_sql);
        
        //编译sql
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(db_,insert_sql,-1,&stmt,nullptr);
        if(rc!=SQLITE_OK)
        {
            LogError(getLogger(),"prepared sql failed! error message: %s",sqlite3_errmsg(db_));
            return false;
        }

        //绑定参数
        //字符串在sql执行期间都是有效的，所以使用SQLITE_STATIC
        sqlite3_bind_text(stmt,1,info.url_.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_int64(stmt,2,static_cast<sqlite3_int64>(info.atime_));
        sqlite3_bind_int64(stmt,3,static_cast<sqlite3_int64>(info.mtime_));
        sqlite3_bind_text(stmt,4,info.storage_path_.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_int64(stmt,5,static_cast<sqlite3_int64>(info.file_size_));

        //执行sql
        rc=sqlite3_step(stmt);
        //清理内存
        sqlite3_finalize(stmt);

        if(rc!=SQLITE_DONE)
        {
            LogError(getLogger(),"execute sql failed! error message: %s",sqlite3_errmsg(db_));
            return false;
        }
        return true;
    }

    bool update(const StorageInfo &info)
    {
        return insert(info);
    }

    bool getOneByURL(const std::string &url, StorageInfo &info)
    {
        std::shared_lock<std::shared_mutex>lock(mtx_);

        const char* select_sql_by_url="select url, atime, mtime, storage_path, file_size from tem_table tt where url=?;";
        LogInfo(getLogger(),"%s:%s",__FUNCTION__,select_sql_by_url);
        
        //编译sql
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(db_,select_sql_by_url,-1,&stmt,nullptr);
        if(rc!=SQLITE_OK)
        {
            LogError(getLogger(),"prepared sql failed! error message: %s",sqlite3_errmsg(db_));
            return false;
        }
        //绑定参数
        sqlite3_bind_text(stmt,1,url.c_str(),-1,SQLITE_STATIC);

        //执行sql
        if(sqlite3_step(stmt)==SQLITE_ROW)
        {
            info.url_=reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            info.atime_=static_cast<time_t>(sqlite3_column_int64(stmt,1));
            info.mtime_=static_cast<time_t>(sqlite3_column_int64(stmt,2));
            info.storage_path_=reinterpret_cast<const char*>(sqlite3_column_text(stmt,3));
            info.file_size_=static_cast<size_t>(sqlite3_column_int64(stmt,4));
        }
        //清理内存
        sqlite3_finalize(stmt);
        lock.unlock();
        return true;
    }

    bool getOneByStoragePath(const fs::path &storage_path, StorageInfo& info)
    {
        std::shared_lock<std::shared_mutex>lock(mtx_);

        const char* select_sql_by_sp="select url, atime, mtime, storage_path, file_size from tem_table tt where storage_path=?;";
        LogInfo(getLogger(),"%s:%s",__FUNCTION__,select_sql_by_sp);
        
        //编译sql
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(db_,select_sql_by_sp,-1,&stmt,nullptr);
        if(rc!=SQLITE_OK)
        {
            LogError(getLogger(),"prepared sql failed! error message: %s",sqlite3_errmsg(db_));
            return false;
        }
        //绑定参数
        sqlite3_bind_text(stmt,1,storage_path.c_str(),-1,SQLITE_STATIC);

        //执行sql
        if(sqlite3_step(stmt)==SQLITE_ROW)
        {
            info.url_=reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            info.atime_=static_cast<time_t>(sqlite3_column_int64(stmt,1));
            info.mtime_=static_cast<time_t>(sqlite3_column_int64(stmt,2));
            info.storage_path_=reinterpret_cast<const char*>(sqlite3_column_text(stmt,3));
            info.file_size_=static_cast<size_t>(sqlite3_column_int64(stmt,4));
        }
        //清理内存
        sqlite3_finalize(stmt);
        lock.unlock();
        return true;
    }

    bool getAll(std::vector<StorageInfo>&arr)
    {
        std::shared_lock<std::shared_mutex>lock(mtx_);

        const char* select_sql_all="select url, atime, mtime, storage_path, file_size from tem_table tt;";
        LogInfo(getLogger(),"%s:%s",__FUNCTION__,select_sql_all);
        
        //编译sql
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(db_,select_sql_all,-1,&stmt,nullptr);
        if(rc!=SQLITE_OK)
        {
            LogError(getLogger(),"prepared sql failed! error message: %s",sqlite3_errmsg(db_));
            return false;
        }

        //执行sql
        while(sqlite3_step(stmt)==SQLITE_ROW)
        {
            StorageInfo info;
            info.url_=reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
            info.atime_=static_cast<time_t>(sqlite3_column_int64(stmt,1));
            info.mtime_=static_cast<time_t>(sqlite3_column_int64(stmt,2));
            info.storage_path_=reinterpret_cast<const char*>(sqlite3_column_text(stmt,3));
            info.file_size_=static_cast<size_t>(sqlite3_column_int64(stmt,4));
            arr.emplace_back(std::move(info));
        }
        //清理内存
        sqlite3_finalize(stmt);
        lock.unlock();
        return true;
    }

    bool deleteByURL(const std::string &url)
    {
        std::lock_guard<std::shared_mutex>lock(mtx_);
        //准备sql语句
        const char * delete_sql=
            "delete from tem_table where url=?;";
        
        LogInfo(getLogger(),"%s:%s",__FUNCTION__,delete_sql);
        
        //编译sql
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(db_,delete_sql,-1,&stmt,nullptr);
        if(rc!=SQLITE_OK)
        {
            LogError(getLogger(),"prepared sql failed! error message: %s",sqlite3_errmsg(db_));
            return false;
        }
        //绑定参数
        sqlite3_bind_text(stmt,1,url.c_str(),-1,SQLITE_STATIC);
        //执行sql
        rc=sqlite3_step(stmt);
        //清理内存
        sqlite3_finalize(stmt);

        if(rc!=SQLITE_DONE)
        {
            LogError(getLogger(),"execute sql failed! error message: %s",sqlite3_errmsg(db_));
            return false;
        }
        return true;
    }
};

} // namespace mystorage
