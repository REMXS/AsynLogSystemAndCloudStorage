cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)

add_executable(TestCloudStorage test.cc)

target_include_directories(TestCloudStorage PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)

target_link_libraries(TestCloudStorage PRIVATE
    GTest::gtest
    GTest::gmock
    Server_lib
)

add_test(NAME TestCloudStorage COMMAND TestCloudStorage)