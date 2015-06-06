cmake_minimum_required(VERSION 2.8)
project(sdr)

find_library(MIR_LIBRARY NAMES mir_sdr mirsdrapi-rsp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")

set(SOURCE_FILES main.cpp sdrplay.cpp mySocket.cpp error.cpp)
set(HEADER_FILES sdrplay.h mySocket.h error.h)
add_executable(sdr ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(sdr pthread ${MIR_LIBRARY})
