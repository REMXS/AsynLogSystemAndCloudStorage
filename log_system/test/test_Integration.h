#pragma once
#include "test_helper.h"

#include "AsyncLogger.hpp"


namespace fs=std::filesystem;
using namespace asynclog;


class IntegrationTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        
    }
    void SetUp()
    {
        config_content=R"({
        "buffer_size": 10,       
        "threshold": 500,      
        "linear_growth" : 50,
        "flush_log" : 2,
        "backup_addr" : "47.116.74.254",
        "backup_port" : 8080,
        "thread_count" : 3
    })";

        //写入配置文件
        {
            std::ofstream ofs(conf_path);
            ASSERT_TRUE(ofs.is_open());
            ofs.write(config_content.c_str(),config_content.size());
            ofs.close();
        }

        //加载配置文件
        json_data.loadConfig(conf_path.string());

        //初始化线程池
        pool = std::make_unique<ThreadPool>(json_data.thread_count_, 1000);

        //配置AsyncLoggerBuilder
        logger_builder_.setLoggerName("integration_test_logger");
        logger_builder_.setConfig(json_data);
        logger_builder_.addLogFlush<StdOutFlush>();
        logger_builder_.addLogFlush<FileFlush>("./integration_test.log",json_data);
        logger_builder_.addLogFlush<RollFileFlush>("./roll_integration_test",1024*4,json_data);
    }
    void TearDown()
    {
        //删除日志文件
        if(fs::exists(log_file_path_))
        {
            fs::remove(log_file_path_);
        }
        if(fs::exists(roll_log_folder_path_))
        {
            fs::remove_all(roll_log_folder_path_);
        }
        //删除配置文件
        if(fs::exists(conf_path))
        {
            fs::remove(conf_path);
        }
    }

    //配置文件相关
    std::string config_content;
    fs::path conf_path="./log_config.conf";
    Util::JsonUtil::JsonData json_data;

    //日志文件路径
    fs::path log_file_path_ = "./integration_test.log";
    fs::path roll_log_folder_path_ = "./roll_integration_test";

    //线程池
    std::shared_ptr<ThreadPool>pool;

    AsyncLoggerBuilder logger_builder_;
    std::shared_ptr<RollFileFlush>roll_file_flush_ptr;
};

TEST_F(IntegrationTest, basic_integration_test)
{
    //日志的数量
    std::atomic_int32_t log_counter{0};

    std::vector<std::thread> threads;
    const int thread_num = 10;
    const int logs_per_thread = 100;
    
    {
        //创建日志器
        auto logger = logger_builder_.build(pool);

        //启动多个线程写日志
        for(int i=0;i<thread_num;++i)
        {
            threads.emplace_back(std::thread([&logger,&log_counter,logs_per_thread](){
                for(int j=0;j<logs_per_thread;++j)
                {
                    logger->debug(__FILE__,__LINE__,"Log message from thread %d, log number %d",std::this_thread::get_id(),j);
                    logger->info(__FILE__,__LINE__,"Log message from thread %d, log number %d",std::this_thread::get_id(),j);
                    logger->warn(__FILE__,__LINE__,"Log message from thread %d, log number %d",std::this_thread::get_id(),j);
                    logger->error(__FILE__,__LINE__,"Log message from thread %d, log number %d",std::this_thread::get_id(),j);
                    logger->fatal(__FILE__,__LINE__,"Log message from thread %d, log number %d",std::this_thread::get_id(),j);
                    log_counter.fetch_add(1);
                }
            }));
        }

        for(auto& t : threads)
        {
            t.join();
        }
    }
    //验证结果
    //在fileflush中验证文件的行数
    {
        std::ifstream ifs(log_file_path_,std::ios::binary);
        ASSERT_TRUE(ifs.is_open());

        std::size_t lines=0;
        std::string tem;
        while(std::getline(ifs,tem))
        {
            ++lines;
        }
        ASSERT_EQ(lines,log_counter.load()*5); //每条日志有5个级别
        ifs.close();
    }

    //在rollfileflush中验证生成的日志文件数量
    {
        ASSERT_TRUE(fs::exists(roll_log_folder_path_));
        std::size_t count=0;
        for(const auto&entry:fs::directory_iterator(roll_log_folder_path_))
        {
            if(fs::is_regular_file(entry))
            {
                count++;
            }
        }
        //粗略统计，保证可以正常创建多个文件即可
        ASSERT_GE(count,2);
    }

}


