/* xGPU_Hashpipe_databuf.c
 *
 * Routines for creating and accessing main data transfer
 * buffer in shared memory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>
#include "xGpu_Hashpipe_databuf.h"

hashpipe_databuf_t *xGPU_Hashpipe_databuf_create(int instance_id, int databuf_id)
{
    /* Calc databuf sizes */
    size_t header_size = sizeof(hashpipe_databuf_t)
                       + sizeof(xGPU_Hashpipe_header_cache_alignment);
    size_t block_size  = sizeof(xGPU_Hashpipe_block_t);
    int    n_block = N_INPUT_BLOCKS;
    return hashpipe_databuf_create(
        instance_id, databuf_id, header_size, block_size, n_block);
}

boost::shared_ptr<PipelinePacket> readIn(hashpipe_thread_args_t * args){

    int rv;
    const char * status_key = args->thread_desc->skey;
    hashpipe_status_t st = args->st;
    int block_id_in = 0;
    xGPU_Hashpipe_databuf_t *db_in = (xGPU_Hashpipe_databuf_t *)args->ibuf;

    while ((rv=xGPU_Hashpipe_databuf_wait_filled(db_in, block_id_in)) != HASHPIPE_OK) {
        if (rv==HASHPIPE_TIMEOUT) {
            //std::cout << "Timeout" << std::endl;
            continue;
        } else {
            hashpipe_error(__FUNCTION__, "error waiting for filled databuf");
            pthread_exit(NULL);
            break;
        }
    }

    boost::shared_ptr<PipelinePacket> temp_ptr = db_in->block[block_id_in].packet_ptr;
    db_in->block[block_id_in].packet_ptr = nullptr;
    xGPU_Hashpipe_databuf_set_free(db_in, block_id_in);
    return temp_ptr;
}

void writeOut(hashpipe_thread_args_t * args, OutputPacketQueuePtr outPacketQueuePtr){
    int rv;
    const char * status_key = args->thread_desc->skey;
    hashpipe_status_t st = args->st;
    int block_id_out = 0;
    xGPU_Hashpipe_databuf_t *db_out = (xGPU_Hashpipe_databuf_t *)args->obuf;

    for(boost::shared_ptr<PipelinePacket> outPacket: *outPacketQueuePtr){
        while ((rv=xGPU_Hashpipe_databuf_wait_free(db_out, block_id_out)) 
                != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for free databuf");
                pthread_exit(NULL);
                break;
            }
        }

        db_out->block[block_id_out].packet_ptr = outPacket;

        //Indicate the free slot is now ready to be passed
        if(xGPU_Hashpipe_databuf_set_filled(db_out, block_id_out) != HASHPIPE_OK) {
            hashpipe_error(__FUNCTION__, "error waiting for databuf filled call");
            pthread_exit(NULL);
        }
    }
}


void hashpipeProcess(hashpipe_thread_args_t * args, boost::shared_ptr<PipelineStage> pipelineStage){

    while (run_threads()){
        //Get an input packet
        boost::shared_ptr<PipelinePacket> inPacket = readIn(args);

        //Process the packet
        OutputPacketQueuePtr outputPacketQueuePtr = pipelineStage->processPacket(inPacket);

        //Transmit the packets
        writeOut(args,outputPacketQueuePtr);
    }
}