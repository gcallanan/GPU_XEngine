#ifndef _TRANSPOSE_H
#define _TRANSPOSE_H

#include "tbb/flow_graph.h"
#include "global_definitions.h"



class Transpose{
    public:
        Transpose(boost::shared_ptr<XGpuBuffers> xGpuBuffer, int stageIndex);
        void operator()(boost::shared_ptr<PipelinePacket> inPacket, multi_node::output_ports_type &op);

    private:
        boost::shared_ptr<XGpuBuffers> xGpuBuffer; 
        boost::shared_ptr<PacketArmortiser> outPacketArmortiser;
        const int stageIndex;
	
};

#endif
