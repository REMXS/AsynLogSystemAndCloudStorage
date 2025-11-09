cmake_minimum_required(VERSION 3.10.0)

add_library(asynclog INTERFACE)


target_include_directories(asynclog INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(jsoncpp REQUIRED)
target_link_libraries(asynclog INTERFACE jsoncpp)


add_subdirectory(test)