cmake_minimum_required(VERSION 3.5)
project(vfs.sacd)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(Iconv REQUIRED)
find_package(Threads REQUIRED)
if(WIN32)
  find_package(PThreads4W REQUIRED)
endif()

include_directories(${PROJECT_SOURCE_DIR}/lib/libsacd
                    ${PROJECT_SOURCE_DIR}/lib/libsacd/common
                    ${PROJECT_SOURCE_DIR}/lib/libsacd/dstdec
                    ${KODI_INCLUDE_DIR}/..
                    ${iconv_INCLUDE_DIRS})

add_definitions(-D_LARGEFILE64_SOURCE
                -D_FILE_OFFSET_BITS=64
                -D__STDC_FORMAT_MACROS)

add_subdirectory(lib/libsacd)

set(SACD_SOURCES src/RingBuffer.cpp
                 src/SACDFile.cpp)

set(SACD_HEADERS src/RingBuffer.h
                 src/Helpers.h)

if(NOT WIN32)
  set(DEPLIBS sacd ${CMAKE_THREAD_LIBS_INIT} ${ICONV_LIBRARY})
else()
  set(DEPLIBS sacd ${CMAKE_THREAD_LIBS_INIT} ${PTHREADS4W_LIBRARIES} ${ICONV_LIBRARY} ws2_32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
endif()

build_addon(vfs.sacd SACD DEPLIBS)

include(CPack)
