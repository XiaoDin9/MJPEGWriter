# cmake_minimum_required(VERSION <specify CMake version here>)
cmake_minimum_required(VERSION 3.15)
project(jpeg-to-web_master)

set(CMAKE_CXX_STANDARD 14)

#set(OpenCV_DIR "G:\\Tools\\11_opencv\\OpenCV-MinGW-Build-OpenCV-4.5.2-x64")
#find_package(OpenCV REQUIRED)
#include_directories(${OpenCV_INCLUDE_DIRS})
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

include_directories(.)

add_executable(jpeg-to-web_master
        main.cpp
        jpeg-to-web.cpp
        jpeg-to-web.h ./tools/tools.cpp)
target_link_libraries(jpeg-to-web_master ${OpenCV_LIBS} -lpthread)
