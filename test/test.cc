#include <iostream>
#include "test_helper.h"
#include "server/test_Util.h"
#include "server/test_config.h"
#include "server/test_DataManager.h"
#include "server/test_Service.h"


class LogInit: public ::testing::Environment
{
protected:
    void SetUp() override
    {
        mystorage::initServerLog();
    }

    void TearDown() override
    {
    }
};


int main()
{
    ::testing::InitGoogleTest();
    ::testing::AddGlobalTestEnvironment(new LogInit());
    return RUN_ALL_TESTS();
}