#ifndef _XGPU_HASHPIPE_DATABUF_H
#define _XGPU_HASHPIPE_DATABUF_H

#include <stdint.h>
#include <stdio.h>
#include "hashpipe.h"
#include "hashpipe_databuf.h"
#include "PipelinePackets.h"
#include "PipelineStages.h"
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#define CACHE_ALIGNMENT         256
#define N_INPUT_BLOCKS          10000

/* INPUT BUFFER STRUCTURES
  */
typedef struct xGPU_Hashpipe_block_header {
   uint64_t mcnt;                    // mcount of first packet
} xGPU_Hashpipe_block_header_t;

typedef uint8_t xGPU_Hashpipe_header_cache_alignment[
   CACHE_ALIGNMENT - (sizeof(xGPU_Hashpipe_block_header_t)%CACHE_ALIGNMENT)
];

typedef struct xGPU_Hashpipe_block {
   xGPU_Hashpipe_block_header_t header;
   xGPU_Hashpipe_header_cache_alignment padding; // Maintain cache alignment
   boost::shared_ptr<PipelinePacket> packet_ptr;
} xGPU_Hashpipe_block_t;

typedef struct xGPU_Hashpipe_databuf {
   hashpipe_databuf_t header;
   xGPU_Hashpipe_header_cache_alignment padding;
   //hashpipe_databuf_cache_alignment padding; // Maintain cache alignment
   xGPU_Hashpipe_block_t block[N_INPUT_BLOCKS];
} xGPU_Hashpipe_databuf_t;

/*
 * HELPER FUNCTIONS
 */

boost::shared_ptr<PipelinePacket> readIn(hashpipe_thread_args_t * args);
OutputPacketQueuePtr processPacket(boost::shared_ptr<PipelinePacket> packetIn, hashpipe_thread_args_t * args);
void writeOut(hashpipe_thread_args_t * args, OutputPacketQueuePtr outPacketQueuePtr);
void hashpipeProcess(hashpipe_thread_args_t * args, boost::shared_ptr<PipelineStage> pipelineStage);

/*
 * BUFFER FUNCTIONS
 */
hashpipe_databuf_t *xGPU_Hashpipe_databuf_create(int instance_id, int databuf_id);

static inline xGPU_Hashpipe_databuf_t *xGPU_Hashpipe_databuf_attach(int instance_id, int databuf_id)
{
    return (xGPU_Hashpipe_databuf_t *)hashpipe_databuf_attach(instance_id, databuf_id);
}

static inline int xGPU_Hashpipe_databuf_detach(xGPU_Hashpipe_databuf_t *d)
{
    return hashpipe_databuf_detach((hashpipe_databuf_t *)d);
}

static inline void xGPU_Hashpipe_databuf_clear(xGPU_Hashpipe_databuf_t *d)
{
    hashpipe_databuf_clear((hashpipe_databuf_t *)d);
}

static inline int xGPU_Hashpipe_databuf_block_status(xGPU_Hashpipe_databuf_t *d, int block_id)
{
    return hashpipe_databuf_block_status((hashpipe_databuf_t *)d, block_id);
}

static inline int xGPU_Hashpipe_databuf_total_status(xGPU_Hashpipe_databuf_t *d)
{
    return hashpipe_databuf_total_status((hashpipe_databuf_t *)d);
}

static inline int xGPU_Hashpipe_databuf_wait_free(xGPU_Hashpipe_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_free((hashpipe_databuf_t *)d, block_id);
}

static inline int xGPU_Hashpipe_databuf_busywait_free(xGPU_Hashpipe_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_free((hashpipe_databuf_t *)d, block_id);
}

static inline int xGPU_Hashpipe_databuf_wait_filled(xGPU_Hashpipe_databuf_t *d, int block_id)
{
    return hashpipe_databuf_wait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int xGPU_Hashpipe_databuf_busywait_filled(xGPU_Hashpipe_databuf_t *d, int block_id)
{
    return hashpipe_databuf_busywait_filled((hashpipe_databuf_t *)d, block_id);
}

static inline int xGPU_Hashpipe_databuf_set_free(xGPU_Hashpipe_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_free((hashpipe_databuf_t *)d, block_id);
}

static inline int xGPU_Hashpipe_databuf_set_filled(xGPU_Hashpipe_databuf_t *d, int block_id)
{
    return hashpipe_databuf_set_filled((hashpipe_databuf_t *)d, block_id);
}


//Wrapper Functions to simplify calling the 


#endif