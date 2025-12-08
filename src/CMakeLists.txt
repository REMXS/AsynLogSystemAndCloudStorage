cmake_minimum_required(VERSION 3.10.0)


add_subdirectory(server)


add_executable(Server)
target_sources(Server PRIVATE main_server.cc)

target_link_libraries(Server PRIVATE Server_lib)

set_target_properties(Server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/server_app)