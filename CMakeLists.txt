
project(example)
cmake_minimum_required (VERSION 2.8)

add_definitions(-std=c++11)

include_directories(/usr/local/include)

SET(CMAKE_BUILD_TYPE "Debug")

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb3")

aux_source_directory(. src_list)

add_executable (${PROJECT_NAME} ${src_list})
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output) 


find_library(LIBMYSQL mysqlclient SHARED /usr/lib64/mysql)
find_library(LIBEVT event SHARED /usr/lib64)

target_link_libraries(${PROJECT_NAME} ${LIBMYSQL} ${LIBEVT} dl pthread)
