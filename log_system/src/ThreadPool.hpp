#pragma once

#include <mutex>
#include <functional>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <vector>
#include <future>
#include <memory>
#include <optional>


namespace asynclog
{
class ThreadPool
{
private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic_bool started_;
    std::vector<std::thread>threads_;
    std::queue<std::function<void()>>tasks_;
    std::size_t queue_size_;
public:
    ThreadPool(size_t thread_num,size_t queue_num)
        :queue_size_(queue_num)
        ,started_(true)
    {
        for(int i=0;i<thread_num;++i)
        {
            threads_.emplace_back(std::thread([this](){
                while(true)
                {
                    std::unique_lock<std::mutex>lock(mtx_);
                    cv_.wait(lock,[this](){
                        return !tasks_.empty()||!started_.load();
                    });
                    
                    //线程池停止同时所有任务都执行完毕，直接返回
                    if(!started_.load()&&tasks_.empty()) break;

                    std::function<void()>task=tasks_.front();
                    tasks_.pop();
                    lock.unlock();
                    
                    task();
                }
            }));
        }
    }

    ~ThreadPool()
    {
        if(started_) stop();
        for(auto&thread:threads_)
        {
            thread.join();
        }
    }

    ThreadPool(const ThreadPool&)=delete;
    ThreadPool& operator = (const ThreadPool&)=delete;

    void stop()
    {
        started_.store(false);
        cv_.notify_all();
    }

    bool started()
    {
        return started_.load();
    }

    template<typename F,typename... Args>
    auto enqueue(F&&func,Args&& ... args)->std::optional<std::future<std::invoke_result_t<F,Args...>>>
    {
        using return_type = std::invoke_result_t<F,Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(func),std::forward<Args>(args)...));
        std::optional<std::future<return_type>> ret(task->get_future());

        {
            std::lock_guard<std::mutex>lock(mtx_);

            //队列已满或者是线程池已停止，返回空对象
            if(tasks_.size()>=queue_size_||!started_) return std::nullopt;

            tasks_.emplace([task](){(*task)();});
            cv_.notify_one();
        }

        return ret;
    }

    
};



} // namespace asynclog
