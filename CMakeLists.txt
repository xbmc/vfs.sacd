project(vfs.sacd)

cmake_minimum_required(VERSION 2.6)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/lib/libsacd
                    ${PROJECT_SOURCE_DIR}/lib/libsacd/common
                    ${PROJECT_SOURCE_DIR}/lib/libsacd/dstdec
                    ${KODI_INCLUDE_DIR}
                    ${KODIPLATFORM_INCLUDE_DIRS})

add_definitions(-D_LARGEFILE64_SOURCE
                -D_FILE_OFFSET_BITS=64
                -D__STDC_FORMAT_MACROS)

add_subdirectory(lib/libsacd)

set(SACD_SOURCES src/RingBuffer.cpp
                 src/SACDFile.cpp)

set(DEPLIBS sacd)

build_addon(vfs.sacd SACD DEPLIBS)
