#include <pthread.h>
#include "hashpipe.h"
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include "xGpu_Hashpipe_databuf.h"

int8 num1;

static int init(hashpipe_thread_args_t * args)
{
        hashpipe_status_t st = args->st;
        num1 = 0;
        //selecting a port to listen to
        std::cout << "Thread initialised" << std::endl;
        hashpipe_status_lock_safe(&st);
        hgeti8(st.buf, "NUM1", &num1);
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
    int block_id_in = 0;
    int block_id_out = 0;
    xGPU_Hashpipe_databuf_t *db_in = (xGPU_Hashpipe_databuf_t *)args->ibuf;
    static int temp = 0;

    while (run_threads()){

        //Get a free slot in the output queue

        while ((rv=xGPU_Hashpipe_databuf_wait_filled(db_in, block_id_in)) != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                hashpipe_status_lock_safe(&st);
                hputs(st.buf, status_key, "blocked");
                hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for filled databuf");
                pthread_exit(NULL);
                break;
            }
        }

        int num = db_in->block[block_id_in].packet_ptr->getTimestamp();
        std::cout << "End Stage:" << num << std::endl;

        xGPU_Hashpipe_databuf_set_free(db_in, block_id_in);

    }
}

static hashpipe_thread_desc_t x_GpuHashpipe_end_thread = {
    name: "xGpu_Hashpipe_end_thread",
    skey: "NETSTAT",
    init: init,
    run:  run,
    ibuf_desc: {xGPU_Hashpipe_databuf_create},
    obuf_desc: {NULL}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&x_GpuHashpipe_end_thread);
}