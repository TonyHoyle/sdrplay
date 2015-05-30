cmake_minimum_required(VERSION 2.8)
project(sdr)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp sdrplay.cpp mySocket.cpp error.cpp)
set(HEADER_FILES sdrplay.h mySocket.h error.h)
add_executable(sdr ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(sdr mir_sdr)