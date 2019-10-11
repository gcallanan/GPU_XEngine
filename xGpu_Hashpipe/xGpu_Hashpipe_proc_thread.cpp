#include <pthread.h>
#include "hashpipe.h"
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include "xGpu_Hashpipe_databuf.h"

static int numero = 0;

static int init(hashpipe_thread_args_t * args)
{
        hashpipe_status_t st = args->st;
        //selecting a port to listen to
        std::cout << "Thread initialised" << std::endl;
        hashpipe_status_lock_safe(&st);
        //hgeti8(st.buf, "NUM1", &num1);
        hputi8(st.buf, "NPACKETS", 0);
        hputi8(st.buf, "NBYTES", 0);
        hashpipe_status_unlock_safe(&st);
        return 0;

}

static void *run(hashpipe_thread_args_t * args)
{ 
    int rv;
    const char * status_key = args->thread_desc->skey;
    hashpipe_status_t st = args->st;
    int block_id_out = 0;
    xGPU_Hashpipe_databuf_t *db_out  = (xGPU_Hashpipe_databuf_t *)args->obuf;
    static int temp = 0;

    while (run_threads()){

        //Get an input packet
        boost::shared_ptr<PipelinePacket> inPacket = readIn(args);

        //Process the packet
        int num = inPacket->getTimestamp();
        std::cout << "Proc Stage:" << num << std::endl;

        OutputPacketQueuePtr outputPacketQueuePtr = boost::make_shared<OutputPacketQueue>();
        outputPacketQueuePtr->push_back(inPacket);

        //Transmit the packets
        writeOut(args,outputPacketQueuePtr);

    }
}

static hashpipe_thread_desc_t x_GpuHashpipe_proc_thread = {
    name: "xGpu_Hashpipe_proc_thread",
    skey: "PROCTHREAD",
    init: init,
    run:  run,
    ibuf_desc: {xGPU_Hashpipe_databuf_create},
    obuf_desc: {xGPU_Hashpipe_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&x_GpuHashpipe_proc_thread);
}