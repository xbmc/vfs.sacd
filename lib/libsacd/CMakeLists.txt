cmake_minimum_required(VERSION 3.5)
project(libsacd)

# Include directory paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/common
                    ${CMAKE_CURRENT_SOURCE_DIR}/id3
                    ${CMAKE_CURRENT_SOURCE_DIR}/dstdec)

# Extra flags for GCC
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

set(SOURCES cuesheet.c
            dsdiff.c
            dsf.c
            dst_decoder_ps3.c
            ioctl.c
            iso_writer.c
            sac_accessor.c
            sacd_input.c
            sacd_pb_stream.c
            sacd_reader.c
            sacd_ripper.pb.c
            scarletbook.c
            scarletbook_helpers.c
            scarletbook_id3.c
            scarletbook_output.c
            scarletbook_print.c
            scarletbook_read.c
            common/charset.c
            common/fileutils.c
            common/log.c
            common/logging.c
            common/pb_decode.c
            common/pb_encode.c
            common/socket.c
            common/timeout.c
            common/usocket.c
            common/utils.c
            common/wsocket.c
            id3/id3.c
            id3/id3_frame.c
            id3/id3_frame_content.c
            id3/id3_frame_text.c
            id3/id3_frame_url.c
            id3/id3_tag.c
            id3/unicode.c
            dstdec/buffer_pool.c
            dstdec/ccp_calc.c
            dstdec/dst_ac.c
            dstdec/dst_data.c
            dstdec/dst_decoder.c
            dstdec/dst_fram.c
            dstdec/dst_init.c
            dstdec/unpack_dst.c
            dstdec/yarn.c)

add_library(sacd STATIC ${SOURCES})
