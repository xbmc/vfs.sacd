/**
 * SACD Ripper - https://github.com/sacd-ripper/
 *
 * Copyright (c) 2010-2015 by respective authors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef DST_DECODER_H
#define DST_DECODER_H

#include <stdint.h>
#include "buffer_pool.h"

/* decode or write job (passed from decode list to write list) -- if seq is
   equal to -1, decode_thread is instructed to return; if more is false then
   this is the last chunk, which after writing tells write_thread to return */
typedef struct job_t
{
    long seq;                                 /* sequence number */
    int error;                                /* an error code (eg. DST decoding error) */
    int more;                                 /* true if this is not the last chunk */
    buffer_pool_space_t *in;                  /* input DST data to decode */
    buffer_pool_space_t *out;                 /* resulting DSD decoded data */
    struct job_t *next;                       /* next job in the list (either list) */
} 
job_t;

typedef void (*frame_decoded_callback_t)(uint8_t* frame_data, size_t frame_size, void *userdata);
typedef void (*frame_error_callback_t)(int frame_count, int frame_error_code, const char *frame_error_message, void *userdata);

typedef struct dst_decoder_s
{
    int procs;            /* maximum number of compression threads (>= 1) */
    int channel_count;

    int sequence;       /* each job get's a unique sequence number */

    /* input and output buffer pools */
    buffer_pool_t in_pool;
    buffer_pool_t out_pool;

    /* list of decode jobs (with tail for appending to list) */
    lock *decode_have;   /* number of decode jobs waiting */
    job_t *decode_head, **decode_tail;

    /* list of write jobs */
    lock *write_first;    /* lowest sequence number in list */
    job_t *write_head;

    /* number of decoding threads running */
    int cthreads;

    /* write thread if running */
    thread *writeth;

    frame_decoded_callback_t frame_decoded_callback;
    frame_error_callback_t frame_error_callback;
    void *userdata;
} dst_decoder_t;

dst_decoder_t* dst_decoder_create(int channel_count, frame_decoded_callback_t frame_decoded_callback, frame_error_callback_t frame_error_callback, void *userdata);
void dst_decoder_destroy(dst_decoder_t *dst_decoder);
void dst_decoder_decode(dst_decoder_t *dst_decoder, uint8_t* frame_data, size_t frame_size);


#endif /* DST_DECODER_H */
