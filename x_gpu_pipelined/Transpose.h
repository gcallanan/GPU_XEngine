#ifndef _TRANSPOSE_H
#define _TRANSPOSE_H

#include "global_definitions.h"

#include "PipelinePackets.h"
#include "PipelineStages.h"
#include "XGpuBufferManager.h"


class Transpose : public PipelineStage{
    public:
        Transpose(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager, int stageIndex);
        OutputPacketQueuePtr processPacket(boost::shared_ptr<PipelinePacket> inPacket);

    private:
        boost::shared_ptr<XGpuBufferManager> xGpuBufferManager; 
        const int stageIndex;
	
};

#endif
