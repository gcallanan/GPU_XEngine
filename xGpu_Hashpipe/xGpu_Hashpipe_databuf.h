#ifndef _XGPU_HASHPIPE_DATABUF_H
#define _XGPU_HASHPIPE_DATABUF_H

#include <stdint.h>
#include <stdio.h>
#include "hashpipe.h"
#include "hashpipe_databuf.h"

#define CACHE_ALIGNMENT         256
#define N_INPUT_BLOCKS          100 
#define N_OUTPUT_BLOCKS         100

/* INPUT BUFFER STRUCTURES
  */
typedef struct demo2_input_block_header {
   uint64_t mcnt;                    // mcount of first packet
} demo2_input_block_header_t;

typedef uint8_t demo2_input_header_cache_alignment[
   CACHE_ALIGNMENT - (sizeof(demo2_input_block_header_t)%CACHE_ALIGNMENT)
];

typedef struct demo2_input_block {
   demo2_input_block_header_t header;
   demo2_input_header_cache_alignment padding; // Maintain cache alignment
   uint64_t test;
} demo2_input_block_t;

typedef struct demo2_input_databuf {
   hashpipe_databuf_t header;
   demo2_input_header_cache_alignment padding;
   //hashpipe_databuf_cache_alignment padding; // Maintain cache alignment
   demo2_input_block_t block[N_INPUT_BLOCKS];
} demo2_input_databuf_t;


/*
  * OUTPUT BUFFER STRUCTURES
  */
typedef struct demo2_output_block_header {
   uint64_t mcnt;
} demo2_output_block_header_t;

typedef uint8_t demo2_output_header_cache_alignment[
   CACHE_ALIGNMENT - (sizeof(demo2_output_block_header_t)%CACHE_ALIGNMENT)
];

typedef struct demo2_output_block {
   demo2_output_block_header_t header;
   demo2_output_header_cache_alignment padding; // Maintain cache alignment
   uint64_t sum;
} demo2_output_block_t;

typedef struct demo2_output_databuf {
   hashpipe_databuf_t header;
   demo2_output_header_cache_alignment padding;
   //hashpipe_databuf_cache_alignment padding; // Maintain cache alignment
   demo2_output_block_t block[N_OUTPUT_BLOCKS];
} demo2_output_databuf_t;

/*
 * INPUT BUFFER FUNCTIONS
 */
hashpipe_databuf_t *demo2_input_databuf_create(int instance_id, int databuf_id);

static inline demo2_input_databuf_t *demo2_input_databuf_attach(int instance_id, int databuf_id)
{
    return (demo2_input_databuf_t *)hashpipe_databuf_attach(instance_id, databuf_id);
}

static inline int demo2_input_databuf_detach(demo2_input_databuf_t *d)
{
    return hashpipe_databuf_detach((hashpipe_databuf_t *)d);
}

static inline void demo2_input_databuf_clear(demo2_input_databuf_t *d)
{
    hashpipe_databuf_clear((hashpipe_databuf_t *)d);
}

static inline int demo2_input_databuf_block_status(demo2_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_block_status((hashpipe_databuf_t *)d, block_id);
}

static inline int demo2_input_databuf_total_status(demo2_input_databuf_t *d)
{
    return hashpipe_databuf_total_status((hashpipe_databuf_t *)d);
}

static inline int demo2_input_databuf_wait_free(demo2_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_free((hashpipe_databuf_t *)d, block_id);
}

static inline int demo2_input_databuf_busywait_free(demo2_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_free((hashpipe_databuf_t *)d, block_id);
}

static inline int demo2_input_databuf_wait_filled(demo2_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int demo2_input_databuf_busywait_filled(demo2_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int demo2_input_databuf_set_free(demo2_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_free((hashpipe_databuf_t *)d, block_id);
}

static inline int demo2_input_databuf_set_filled(demo2_input_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_filled((hashpipe_databuf_t *)d, block_id);
}

#endif