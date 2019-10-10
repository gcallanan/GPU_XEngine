#include <pthread.h>
#include "hashpipe.h"
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include "xGpu_Hashpipe_databuf.h"



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
    int block_id_in = 0;
    int block_id_out = 0;
    demo2_input_databuf_t *db_in = (demo2_input_databuf_t *)args->ibuf;
    demo2_input_databuf_t *db_out  = (demo2_input_databuf_t *)args->obuf;
    static int temp = 0;

    while (run_threads()){

        //Get a free slot in the output queue

        while ((rv=demo2_input_databuf_wait_filled(db_in, block_id_in)) != HASHPIPE_OK) {
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
        boost::shared_ptr<PipelinePacket> temp_ptr = db_in->block[block_id_in].packet_ptr;
        std::cout << "Proc Stage:" << num << std::endl;

        demo2_input_databuf_set_free(db_in, block_id_in);

        while ((rv=demo2_input_databuf_wait_free(db_out, block_id_out)) 
                != HASHPIPE_OK) {
            if (rv==HASHPIPE_TIMEOUT) {
                    hashpipe_status_lock_safe(&st);
                    hputs(st.buf, status_key, "blocked");
                    hashpipe_status_unlock_safe(&st);
                continue;
            } else {
                hashpipe_error(__FUNCTION__, "error waiting for free databuf");
                pthread_exit(NULL);
                break;
            }
        }

        db_out->block[block_id_out].test = num++;
        db_out->block[block_id_out].packet_ptr = temp_ptr;

        //Indicate the free slot is now ready to be passed
        if(demo2_input_databuf_set_filled(db_out, block_id_out) != HASHPIPE_OK) {
            hashpipe_error(__FUNCTION__, "error waiting for databuf filled call");
            pthread_exit(NULL);
        }

    }
}

static hashpipe_thread_desc_t x_GpuHashpipe_proc_thread = {
    name: "xGpu_Hashpipe_proc_thread",
    skey: "NETSTAT",
    init: init,
    run:  run,
    ibuf_desc: {demo2_input_databuf_create},
    obuf_desc: {demo2_input_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&x_GpuHashpipe_proc_thread);
}