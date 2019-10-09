#ifndef _GPUWRAPPER_H
#define _GPUWRAPPER_H

#define MINIMUM_QUEUE_SIZE 100

#include "tbb/flow_graph.h"
#include "global_definitions.h"
#include "xgpu.h"
#include <queue>
#include "XGpuBufferManager.h"
#include "PipelinePackets.h"


class GPUWrapper{
    public:
        GPUWrapper(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager);
        void operator()(boost::shared_ptr<PipelinePacket> inPacket, multi_node::output_ports_type &op);
        void setAccumulationsThreshold(int accumulationsThreshold);
        int getAccumulationsThreshold();
    
    private:
        int numAccumulations;
        int accumulationsThreshold;
        const int syncOp = SYNCOP_SYNC_TRANSFER;
        const int finalSyncOp = SYNCOP_DUMP;
        boost::shared_ptr<GPUWrapperPacket> tempGpuWrapperPacket;
        boost::shared_ptr<XGpuBufferManager> xGpuBufferManager;
        std::queue<boost::shared_ptr<TransposePacket>> storageQueue;
        int64_t oldest_timestamp;    
};

#endif