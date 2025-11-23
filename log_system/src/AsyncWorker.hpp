#pragma once

#include <condition_variable>
#include <thread>
#include <mutex>
#include <functional>
#include <iostream>
#include <atomic>
#include <chrono>

#include "AsyncBuffer.hpp"
#include "Util.hpp"

namespace asynclog
{

//缓冲区的策略
enum class BufferPolicy
{
    LIMIT_SIZE,   //缓冲区有限制大小
    UNLIMITED     //缓冲区无限制大小
};

class AsyncWorker
{
private:
    using Functor=std::function<void(Buffer&)>;

    BufferPolicy buffer_policy_;    //是否限制缓冲区大小
    size_t max_buffer_bytes_ ;      //如果限制缓冲区大小，允许写入缓冲区的最大大小(如果不限制大小，则此参数无意义) 
    std::atomic_bool started;       //是否开始工作
    Buffer productor_buffer_;       //生产者缓冲区(用于业务线程写入内容)
    Buffer consumer_buffer_;        //消费者缓冲区(用于后台线程进行将日志内容输出)
    Functor functor_;               //用于处理消费缓冲区内容的函数(将输出缓冲区中的内容写入到其它地方)
    double swap_factor=0.5;         //决定判断缓冲区是否置换的因子(可读数据和缓冲区大小*swap_factor作比较)

    
    std::unique_ptr<std::thread>thread_ ;//后台线程
    //用于消费者线程的条件唤醒和生产者的并发访问
    std::mutex mtx_;
    std::condition_variable cond_consumer_;


    /* 判断是否达到消费要求
    如果生产者缓冲区中的可读的数据达到总量的一部分(由swap_factor决定)则置换 */
    inline bool needSwap()const 
    {
        return productor_buffer_.readableBytes()>productor_buffer_.size()*swap_factor;
    }

    //functor进行一次刷盘应该将缓冲区的数据全部刷入磁盘
    void ThreadEntry()
    {
       while(1)
       {
            std::unique_lock<std::mutex>lock(mtx_);
            //超过3s或者是达到交换阈值时就执行交换将数据刷新到磁盘中
            cond_consumer_.wait_for(lock,std::chrono::seconds(3),[this](){
                return !started||needSwap();
            });

            //如果停止同时缓冲区中无数据的话，退出
            if(!started&&consumer_buffer_.isEmpty()&&productor_buffer_.isEmpty()) break;
            
            productor_buffer_.swap(consumer_buffer_);

            lock.unlock();

            functor_(consumer_buffer_);
            consumer_buffer_.reset();
       }
    }
    
public:
    AsyncWorker(const Util::JsonUtil::JsonData&config_data,Functor functor,
        BufferPolicy buffer_policy=BufferPolicy::UNLIMITED,size_t max_buffer_bytes=16*1024)
        :buffer_policy_(buffer_policy)
        ,max_buffer_bytes_(max_buffer_bytes)
        ,functor_(std::move(functor))
        ,productor_buffer_(config_data)
        ,consumer_buffer_(config_data)
        ,started(false)
    {}
    ~AsyncWorker()
    {

        stop();
        if(thread_->joinable())
        {
            thread_->join();
        }
    }

    //线程安全
    bool push(const char* data,size_t len)
    {
        if(data==nullptr) return false;
        bool need_notify=false;
         {
            std::lock_guard<std::mutex>lock(mtx_);
            if(!started) return false;

            if(buffer_policy_==BufferPolicy::LIMIT_SIZE)
            {
                if(productor_buffer_.readableBytes()+len>max_buffer_bytes_)
                {
                    return false;
                }   
            }
            //写入日志
            productor_buffer_.push(data,len);

            //检查是否需要消费者消费
            if(needSwap())
            {
                need_notify=true;
            }
        }

        if(need_notify) cond_consumer_.notify_one();
        return true;
    }

    void start()
    {
        started.store(true);
        thread_=std::make_unique<std::thread>([this](){this->ThreadEntry();});
    }

    void stop()
    {
        started.store(false);
        cond_consumer_.notify_all();
    }
};

} // namespace asynclog

