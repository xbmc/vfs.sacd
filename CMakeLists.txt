cmake_minimum_required(VERSION 3.5)
project(vfs.sacd)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(Iconv REQUIRED)
find_package(Threads REQUIRED)
find_package(WavPack REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/lib/libdsdpcm
                    ${PROJECT_SOURCE_DIR}/lib/libdstdec
                    ${PROJECT_SOURCE_DIR}/lib/libdstdec/binding
                    ${PROJECT_SOURCE_DIR}/lib/libdstdec/decoder
                    ${KODI_INCLUDE_DIR}/..
                    ${iconv_INCLUDE_DIRS}
                    ${WAVPACK_INCLUDE_DIRS})

add_definitions(-D_LARGEFILE64_SOURCE
                -D_FILE_OFFSET_BITS=64
                -D__STDC_FORMAT_MACROS)

add_subdirectory(lib/libdsdpcm)
add_subdirectory(lib/libdstdec)

set(SACD_SOURCES src/Addon.cpp
                 src/RingBuffer.cpp
                 src/SACDAudio.cpp
                 src/SACDFile.cpp
                 src/Settings.cpp
                 src/sacd/id3_tagger.cpp
                 src/sacd/sacd_core.cpp
                 src/sacd/sacd_disc.cpp
                 src/sacd/sacd_dsd.cpp
                 src/sacd/sacd_dsdiff.cpp
                 src/sacd/sacd_dsf.cpp
                 src/sacd/sacd_media.cpp
                 src/sacd/sacd_wavpack.cpp
                 src/sacd/scarletbook.cpp)

set(SACD_HEADERS src/Addon.h
                 src/RingBuffer.h
                 src/SACDAudio.h
                 src/SACDFile.h
                 src/Settings.h
                 src/Helpers.h
                 src/sacd/id3_tagger.h
                 src/sacd/sacd_core.h
                 src/sacd/sacd_disc.h
                 src/sacd/sacd_dsd.h
                 src/sacd/sacd_dsdiff.h
                 src/sacd/sacd_dsf.h
                 src/sacd/sacd_media.h
                 src/sacd/sacd_reader.h
                 src/sacd/sacd_wavpack.h
                 src/sacd/scarletbook.h)

if(NOT WIN32)
  set(DEPLIBS dsdpcm dstdec ${CMAKE_THREAD_LIBS_INIT} ${WAVPACK_LIBRARIES} ${ICONV_LIBRARY})
else()
  set(DEPLIBS dsdpcm dstdec ${CMAKE_THREAD_LIBS_INIT} ${WAVPACK_LIBRARIES} ${ICONV_LIBRARY} ws2_32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
endif()

build_addon(vfs.sacd SACD DEPLIBS)

include(CPack)
