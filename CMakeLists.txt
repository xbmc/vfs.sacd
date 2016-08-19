project(vfs.sacd)

cmake_minimum_required(VERSION 2.6)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(p8-platform REQUIRED)
find_package(Kodiplatform REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/lib/libsacd
                    ${PROJECT_SOURCE_DIR}/lib/libsacd/common
                    ${PROJECT_SOURCE_DIR}/lib/libsacd/dstdec
                    ${KODI_INCLUDE_DIR}
                    ${p8-platform_INCLUDE_DIRS}
                    ${kodiplatform_INCLUDE_DIRS})

add_definitions(-D_LARGEFILE64_SOURCE
                -D_FILE_OFFSET_BITS=64
                -D__STDC_FORMAT_MACROS)

add_subdirectory(lib/libsacd)

set(SACD_SOURCES src/RingBuffer.cpp
                 src/SACDFile.cpp)

set(DEPLIBS sacd ${p8-platform_LIBRARIES} ${kodiplatform_LIBRARIES})

build_addon(vfs.sacd SACD DEPLIBS)
