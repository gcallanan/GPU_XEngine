#ifndef _GPUWRAPPER_H
#define _GPUWRAPPER_H

#define MINIMUM_QUEUE_SIZE 100

#include "global_definitions.h"
#include "xgpu.h"
#include <queue>
#include "XGpuBufferManager.h"
#include "PipelinePackets.h"
#include "PipelineStages.h"


class GPUWrapper : public PipelineStage{
    public:
        GPUWrapper(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager);
        GPUWrapper(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager,int accumulationsThreshold);
        OutputPacketQueuePtr processPacket(boost::shared_ptr<PipelinePacket> inPacket);
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