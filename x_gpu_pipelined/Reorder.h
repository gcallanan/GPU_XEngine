#include <deque>
#include "global_definitions.h"
#include <cstdio>
#include "tbb/flow_graph.h"

#ifndef _REORDER_H
#define _REORDER_H

#define BUFFER_SIZE 20
#define PACKET_THRESHOLD_BEFORE_SYNC 20 
#define TIMESTAMP_JUMP (NUM_TIME_SAMPLES*2*FFT_SIZE)

class Reorder{
    public:
        Reorder();
        void operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op);

    private:
        std::deque<boost::shared_ptr<ReorderPacket> > buffer;
        uint64_t first_timestamp;
        
};

#endif

