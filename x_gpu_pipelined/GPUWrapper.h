#ifndef _GPUWRAPPER_H
#define _GPUWRAPPER_H

#define MINIMUM_QUEUE_SIZE 100

#include "tbb/flow_graph.h"
#include "global_definitions.h"
#include "xgpu.h"
#include <queue>


class GPUWrapper{
    public:
        GPUWrapper(boost::shared_ptr<XGpuBuffers> xGpuBuffer);
        void operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op);
        void setAccumulationsThreshold(int accumulationsThreshold);
        int getAccumulationsThreshold();
    
    private:
        int numAccumulations;
        int accumulationsThreshold;
        const int syncOp = SYNCOP_SYNC_TRANSFER;
        const int finalSyncOp = SYNCOP_DUMP;
        boost::shared_ptr<GPUWrapperPacket> tempGpuWrapperPacket;
        boost::shared_ptr<XGpuBuffers> xGpuBuffer;
        std::queue<boost::shared_ptr<ReorderPacket>> storageQueue;
        std::priority_queue<boost::shared_ptr<StreamObject>,std::vector<boost::shared_ptr<StreamObject>>,StreamObjectPointerCompare> inputOrderedQueue;
        int64_t oldest_timestamp;    
};

#endif