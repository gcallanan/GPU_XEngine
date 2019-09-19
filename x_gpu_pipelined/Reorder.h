#ifndef _REORDER_H
#define _REORDER_H

#include "tbb/flow_graph.h"
#include "global_definitions.h"

#define NUM_REORDER_STAGES 2
#define BLOCK_SIZE 16//Must be a power of 2
#define USE_SSE 1 //1 to use, 0 to not use

class Reorder{
    public:
        Reorder(boost::shared_ptr<XGpuBuffers> xGpuBuffer, int stageIndex);
        void operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op);

    private:
        boost::shared_ptr<XGpuBuffers> xGpuBuffer; 
        boost::shared_ptr<Spead2RxPacketWrapper> outPacketArmortiser;
        const int stageIndex;
	
};

#endif
