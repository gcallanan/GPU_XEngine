#include <pthread.h>
#include "hashpipe.h"
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include "xGpu_Hashpipe_databuf.h"
#include "Buffer.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

static int numero = 0;

boost::shared_ptr<Buffer> bufferStagePtr;

static int init(hashpipe_thread_args_t * args)
{
        hashpipe_status_t st = args->st;
        bufferStagePtr = boost::make_shared<Buffer>();
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
    hashpipeProcess(args,bufferStagePtr);
}

static hashpipe_thread_desc_t buffer_thread = {
    name: "buffer_thread",
    skey: "buffer_thread",
    init: init,
    run:  run,
    ibuf_desc: {xGPU_Hashpipe_databuf_create},
    obuf_desc: {xGPU_Hashpipe_databuf_create}
};

static __attribute__((constructor)) void ctor()
{
  register_hashpipe_thread(&buffer_thread);
}