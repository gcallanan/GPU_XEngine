#ifndef _GPUWRAPPER_H
#define _GPUWRAPPER_H

#define MINIMUM_QUEUE_SIZE 100

#include "global_definitions.h"
#include "xgpu.h"
#include <deque>
#include "XGpuBufferManager.h"
#include "PipelinePackets.h"
#include "PipelineStages.h"


class GPUWrapper : public PipelineStage{
    public:
        GPUWrapper(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager, int64_t syncStart);
        GPUWrapper(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager,int64_t syncStart ,int accumulationsThreshold);
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
        std::deque<boost::shared_ptr<TransposePacket>> storageQueue;
        int64_t oldest_timestamp;   
        int64_t syncStart; 
        int64_t syncTimestamp;
        int64_t missingTimestampsThisAccumulation;
        bool synced;
};

#endif