#ifndef _REORDER_H
#define _REORDER_H

#include "tbb/flow_graph.h"
#include "global_definitions.h"



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
