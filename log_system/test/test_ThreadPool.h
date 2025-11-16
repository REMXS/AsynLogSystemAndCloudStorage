#pragma once
#include "test_helper.h"
#include "ThreadPool.hpp"

#include <vector>


class ThreadPoolTest:public ::testing::Test
{
public:
    int test_func(int a)
    {
        cnt+=a;
        return a;
    }
protected:
    size_t cnt=0;
    
};




TEST_F(ThreadPoolTest,base_test)
{
    using namespace asynclog;
    ThreadPool pool(3,2);
    int a=1;
    auto res=pool.enqueue(&ThreadPoolTest::test_func,this,a);

    int ret=res->get();
    ASSERT_EQ(cnt,1);
    ASSERT_EQ(ret,1);
}

TEST_F(ThreadPoolTest,full_task_test)
{
    using namespace asynclog;
    ThreadPool pool(3,0);
    int a=1;
    pool.enqueue(&ThreadPoolTest::test_func,this,a);
    auto res=pool.enqueue(&ThreadPoolTest::test_func,this,a);
    ASSERT_EQ(res,std::nullopt);
}
TEST_F(ThreadPoolTest,stop_test)
{
    using namespace asynclog;
    ThreadPool pool(3,3);
    pool.stop();
    int a=1;
    auto res=pool.enqueue(&ThreadPoolTest::test_func,this,a);
    ASSERT_EQ(res,std::nullopt);
}

TEST_F(ThreadPoolTest,stress_test)
{
    using namespace asynclog;
    ThreadPool pool(16,1001);
    int a=1;
    std::vector<std::optional<std::future<int>>>vec(1000);
    for(int i=0;i<vec.size();++i)
    {
        vec[i]=pool.enqueue(&ThreadPoolTest::test_func,this,a);
    }
    for(auto&ret:vec)
    {
        int tem=ret->get();
        ASSERT_EQ(tem,a);
    }
    ASSERT_EQ(cnt,1000);
}
