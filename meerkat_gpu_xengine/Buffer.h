#ifndef _BUFFER_H
#define _BUFFER_H

#include <deque>
#include <cstdio>

#include "global_definitions.h"
#include "PipelinePackets.h"
#include "PipelineStages.h"

#define PACKET_THRESHOLD_BEFORE_SYNC 20 

class Buffer : public PipelineStage{
    public:
        Buffer();
        OutputPacketQueuePtr processPacket(boost::shared_ptr<PipelinePacket> inPacket);

    private:
        std::deque<boost::shared_ptr<BufferPacket> > buffer;
        uint64_t first_timestamp;
};


#endif

