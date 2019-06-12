#ifndef _SPEADTX_H
#define _SPEADTX_H

#include "tbb/flow_graph.h"
#include "global_definitions.h"

class SpeadTx{
    public:
        SpeadTx();
        void operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op);      
};

#endif