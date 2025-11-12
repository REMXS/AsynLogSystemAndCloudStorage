cmake_minimum_required(VERSION 3.10.0)
project(AsynLogSystem_CloudStorage VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


add_subdirectory(log_system)
add_subdirectory(src)
