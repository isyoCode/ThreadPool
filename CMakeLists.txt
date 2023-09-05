cmake_minimum_required(VERSION 3.10)
project(Demo)

# include_directories(./threadpool.h)

set(CMAKE_CXX_STANDARD 17)

# 设置文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)



# #先制作动态链接库
add_library(mythread SHARED threadpool.cc)

add_executable(${PROJECT_NAME} main.cpp)


target_link_libraries(${PROJECT_NAME} 
                    pthread
                    mythread)
