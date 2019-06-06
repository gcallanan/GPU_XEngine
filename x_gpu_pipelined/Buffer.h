#include <deque>
#include "global_definitions.h"
#include <cstdio>
#include "tbb/flow_graph.h"

#ifndef _BUFFER_H
#define _BUFFER_H

#define PACKET_THRESHOLD_BEFORE_SYNC 200 
#define TIMESTAMP_JUMP (NUM_TIME_SAMPLES*2*FFT_SIZE)

class Buffer{
    public:
        Buffer();
        void operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op);

    private:
        std::deque<boost::shared_ptr<BufferPacket> > buffer;
        uint64_t first_timestamp;
        
};

#endif

