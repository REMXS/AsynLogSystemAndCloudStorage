cmake_minimum_required(VERSION 3.10.0)



add_executable(Test test.cc)

find_package(GTest REQUIRED)

target_link_libraries(Test PRIVATE GTest::gtest GTest::gmock asynclog)

#add_test(NAME LOG_TEST COMMAND Test)