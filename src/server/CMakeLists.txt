cmake_minimum_required(VERSION 3.10.0)

find_package(PkgConfig REQUIRED)
find_package(jsoncpp REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)

add_library(Server_lib INTERFACE)



target_link_libraries(Server_lib INTERFACE
    event
    asynclog
    ${LIBEVENT_LIBRARIES}
    jsoncpp
)

target_include_directories(Server_lib INTERFACE
    ${LIBEVENT_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

