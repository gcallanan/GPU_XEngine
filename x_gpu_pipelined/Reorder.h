#ifndef _REORDER_H
#define _REORDER_H

#include "tbb/flow_graph.h"
#include "global_definitions.h"

class Reorder{
    public:
        Reorder();
        void operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op);
        
};

#endif