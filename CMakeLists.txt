cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
project(CppDocker)
set(EXECUTABLE_OUTPUT_PATH .build/../bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_BUILD_TYPE Debug)

# include your headers
include_directories(headers)
set(CPP_SOURCES 
    src/logger.cpp 
    src/container.cpp 
    src/main.cpp
)
add_executable(main ${CPP_SOURCES})
# target_compile_options(main PRIVATE -std=c++20)

# 为目标(可执行文件)添加源文件
target_sources(main PRIVATE 
    src/networktool/network.c
    src/networktool/nl.c)
# c与c++分别编译
set_property(TARGET main PROPERTY CXX_STANDARD 20)
set_property(TARGET main PROPERTY C_STANDARD 17)
   

