cmake_minimum_required(VERSION 2.8)
project(sdr)

INCLUDE(FindPkgConfig)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} /home/tmh/sdr)

find_library(MIR_LIBRARY NAMES mir_sdr mirsdrapi-rsp)
find_package(libusb-1.0 REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

set(SOURCE_FILES src/main.cpp src/sdrplay.cpp src/mySocket.cpp src/error.cpp)
set(HEADER_FILES src/sdrplay.h src/mySocket.h src/error.h)

add_executable(sdr ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(sdr pthread ${MIR_LIBRARY})
