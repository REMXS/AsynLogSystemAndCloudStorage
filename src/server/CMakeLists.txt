cmake_minimum_required(VERSION 3.10.0)

find_package(PkgConfig REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(SQLite3 REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED 
    libevent
    libzstd
)

add_library(Server_lib SHARED)

target_sources(Server_lib PRIVATE base64.cpp)

target_link_libraries(Server_lib PUBLIC
    asynclog
    ${LIBEVENT_LIBRARIES}
    ${ZSTD_LIBRARIES}
    jsoncpp
    SQLite::SQLite3
)

target_include_directories(Server_lib PRIVATE
    ${LIBEVENT_INCLUDE_DIRS}
    ${ZSTD_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)


