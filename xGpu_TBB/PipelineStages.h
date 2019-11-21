#ifndef _PIPELINE_STAGE_H
#define _PIPELINE_STAGE_H

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "global_definitions.h"
#include "XGpuBufferManager.h"
#include "PipelinePackets.h"
#include <string>

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
            if(outPacketArmortiser==nullptr){
                outPacketArmortiser=boost::make_shared<PacketArmortiser>();
            }
            boost::shared_ptr<PacketArmortiser> inPacketQueue = boost::dynamic_pointer_cast<PacketArmortiser>(inPacket);
            while(inPacketQueue->getArmortiserSize() > 0){
                boost::shared_ptr<PipelinePacket> inPacket_pop = inPacketQueue->removePacket();
                this->packetsProcessed++;
                OutputPacketQueuePtr outPacketQueue = processPacket(inPacket_pop);
                for(boost::shared_ptr<PipelinePacket> outPacket: *outPacketQueue){
                    outPacketArmortiser->addPacket(boost::dynamic_pointer_cast<PipelinePacket>(outPacket));
                    if(outPacketArmortiser->getArmortiserSize() >= this->armortiserMaxSize){
                        if(!std::get<0>(op).try_put(outPacketArmortiser)){
                            //std::cout << "Packet Failed to be passed to next class" << std::endl;
                        }
                        outPacketArmortiser = boost::make_shared<PacketArmortiser>();
                        outPacket = nullptr;
                    }
                }
            }
            inPacketQueue = nullptr;
        }

        PipelineStage(){
        }

        int getPacketsProcessed(){
            return packetsProcessed;
        }

        std::string getStageName(){
            return stageName;
        }

        std::string toString(){
            return stageName + " " + std::to_string(packetsProcessed);
        }

    private:
        boost::shared_ptr<PacketArmortiser> outPacketArmortiser;
        int packetsProcessed=0;

    protected:
        int armortiserMaxSize = ARMORTISER_SIZE;
        std::string stageName = "";

};

#endif