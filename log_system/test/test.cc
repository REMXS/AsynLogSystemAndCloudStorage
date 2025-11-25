#include "test_helper.h"
#include <iostream>
#include "test_Level.h"
#include "test_Util.h"
#include "test_Message.h"
#include "test_Buffer.h"
#include "test_ThreadPool.h"
#include "test_LogFlush.h"

#include "test_AsyncWorker.h"
#include "test_AsyncLogger.h"
#include "test_Integration.h"


/* TEST(NewProjectTest,base_test)
{
    std::cout<<"hello world, this is a new project"<<std::endl;
} */


int main()
{
    ::testing::InitGoogleTest();

    return RUN_ALL_TESTS();
}
