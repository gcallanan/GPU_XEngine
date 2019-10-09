#ifndef _PIPELINE_STAGE_H
#define _PIPELINE_STAGE_H

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "global_definitions.h"
#include "XGpuBufferManager.h"
#include "PipelinePackets.h"

/**
 * \brief A base class to define a single stage in the pipeline.
 * 
 * This base class does not contain any stage specific variables.
 * 
 * \author Gareth Callanan
 */

typedef boost::shared_ptr<std::deque<boost::shared_ptr<PipelinePacket>>> OutputPacketQueuePtr;
typedef std::deque<boost::shared_ptr<PipelinePacket>> OutputPacketQueue;

class PipelineStage{
    public:
        virtual OutputPacketQueuePtr processPacket(boost::shared_ptr<PipelinePacket> inPacket) = 0;

        void operator()(boost::shared_ptr<PipelinePacket> inPacket, multi_node::output_ports_type &op){
            boost::shared_ptr<PacketArmortiser> inPacketQueue = boost::dynamic_pointer_cast<PacketArmortiser>(inPacket);
            

            while(inPacketQueue->getArmortiserSize() > 0){
                boost::shared_ptr<PipelinePacket> inPacket_pop = inPacketQueue->removePacket();
                OutputPacketQueuePtr outPacketQueue = processPacket(inPacket_pop);
                for(boost::shared_ptr<PipelinePacket> outPacket: *outPacketQueue){
                    outPacket->getTimestamp();
                    outPacketArmortiser->addPacket(boost::dynamic_pointer_cast<PipelinePacket>(outPacket));
                    if(outPacketArmortiser->getArmortiserSize() >= this->armortiserMaxSize){
                        if(!std::get<0>(op).try_put(outPacketArmortiser)){
                            //std::cout << "Packet Failed to be passed to reorder class" << std::endl;
                        }
                        outPacketArmortiser = boost::make_shared<PacketArmortiser>();
                    }
                }
                
            }
        }

        PipelineStage(){
            outPacketArmortiser = boost::make_shared<PacketArmortiser>();
        }

    private:
        boost::shared_ptr<PacketArmortiser> outPacketArmortiser;
        int armortiserMaxSize = ARMORTISER_SIZE;

};

#endif