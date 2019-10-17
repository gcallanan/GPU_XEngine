#include <pthread.h>
#include "hashpipe.h"
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include "xGpu_Hashpipe_databuf.h"

int8 num;

static int init(hashpipe_thread_args_t * args)
{
        hashpipe_status_t st = args->st;
        num = 0;
        //selecting a port to listen to
        std::cout << "Thread initialised" << std::endl;
        // hashpipe_status_lock_safe(&st);
        // hgeti8(st.buf, "NUM", &num);
        // hputi8(st.buf, "NPACKETS", 0);
        // hputi8(st.buf, "NBYTES", 0);
        // hashpipe_status_unlock_safe(&st);
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
        //Get a free slot in the output queue
        std::this_thread::sleep_for(std::chrono::seconds(1));
        while ((rv=xGPU_Hashpipe_databuf_wait_free(db_out, block_id_out)) 
                != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                    //hashpipe_status_lock_safe(&st);
                    //hputs(st.buf, status_key, "blocked");
                    //hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for free databuf");
                pthread_exit(NULL);
                break;
            }
        }

        boost::shared_ptr<BufferPacket> temp_ptr = boost::make_shared<BufferPacket>(++temp,false,0);
        db_out->block[block_id_out].packet_ptr = temp_ptr;
        std::cout << "Input Stage: " << temp << std::endl; 

        //Indicate the free slot is now ready to be passed
        if(xGPU_Hashpipe_databuf_set_filled(db_out, block_id_out) != HASHPIPE_OK) {
            hashpipe_error(__FUNCTION__, "error waiting for databuf filled call");
            pthread_exit(NULL);
        }
    }
}

static hashpipe_thread_desc_t x_GpuHashpipe_input_thread = {
    name: "xGpu_Hashpipe_input_thread",
    skey: "NETSTAT",
    init: init,
    run:  run,
    ibuf_desc: {NULL},
    obuf_desc: {xGPU_Hashpipe_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&x_GpuHashpipe_input_thread);
}