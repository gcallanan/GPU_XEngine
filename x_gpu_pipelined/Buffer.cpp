#include "Buffer.h"
#include "global_definitions.h"
#include <iostream>



Buffer::Buffer(): first_timestamp(0){
    outPacketArmortiser = boost::make_shared<Spead2RxPacketWrapper>();
    for (size_t i = 0; i < BUFFER_SIZE; i++)
    {
        buffer.push_front(nullptr);
    }
}

void Buffer::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    boost::shared_ptr<Spead2RxPacketWrapper> inPacketQueue = boost::dynamic_pointer_cast<Spead2RxPacketWrapper>(inPacket);
    

    while(inPacketQueue->getArmortiserSize() > 0){
        boost::shared_ptr<StreamObject> inPacket_pop = inPacketQueue->removePacket();
        boost::shared_ptr<Spead2RxPacket> inPacket_cast = boost::dynamic_pointer_cast<Spead2RxPacket>(inPacket_pop);
        uint64_t packet_timestamp = inPacket_cast->getTimestamp();
        
        if((packet_timestamp - first_timestamp)/TIMESTAMP_JUMP > RESYNC_LIMIT && (((int64_t)packet_timestamp - (int64_t)first_timestamp)/TIMESTAMP_JUMP) > 0){
            std::cout << "Timestamp off by "<<(((int64_t)packet_timestamp - (int64_t)first_timestamp)/TIMESTAMP_JUMP)<<" samples, resync triggered in Buffer class" << std::endl;
            first_timestamp = 0;
        }

        int index = ((int64_t)packet_timestamp - (int64_t)first_timestamp)/TIMESTAMP_JUMP;
        if(first_timestamp>packet_timestamp){
            //std::cout << "Timestamp smaller than minimum received in Buffer class by "<< index <<", Not Keeping Up" << std::endl;
            pipelineCounts.packetsTooLate++;
        }else{
            if(index>BUFFER_SIZE+PACKET_THRESHOLD_BEFORE_SYNC){//Packet Far Outside of Range
                for (size_t i = 0; i < BUFFER_SIZE; i++)
                {
                    buffer.push_front(nullptr);
                    buffer.pop_back();
                }
                buffer[0] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                buffer[0]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                first_timestamp = packet_timestamp;
            }else if(index>=BUFFER_SIZE){//Packet just outside of range
                int numPops = 0;
                while((index>=BUFFER_SIZE || buffer[0] == nullptr) && numPops != BUFFER_SIZE){
                    if(buffer[0] != nullptr){

                        outPacketArmortiser->addPacket(boost::dynamic_pointer_cast<StreamObject>(buffer[0]));
                        if(outPacketArmortiser->getArmortiserSize() >= ARMORTISER_SIZE){
                            if(!std::get<0>(op).try_put(outPacketArmortiser)){
                                //std::cout << "Packet Failed to be passed to reorder class" << std::endl;
                            }
                            outPacketArmortiser = boost::make_shared<Spead2RxPacketWrapper>();
                        }
                    }
                    buffer.pop_front();
                    buffer.push_back(nullptr);
                    if(buffer[0]!=nullptr){
                        first_timestamp = buffer[0]->getTimestamp();
                        index = (packet_timestamp - first_timestamp)/TIMESTAMP_JUMP;
                    }
                    numPops++;
                }
                if(numPops == BUFFER_SIZE){
                    buffer[0] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                    buffer[0]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                    first_timestamp = buffer[0]->getTimestamp();
                }else if(numPops > BUFFER_SIZE){
                    std::cout << "Buffer Class numPops > BUFFER_SIZE" << std::endl;
                }else{
                    buffer[index] = nullptr;
                    buffer[index] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                    buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                }
            }else if(buffer[index] == nullptr){
                buffer[index] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
            }else{
                buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
            }
        }
    }
    pipelineCounts.BufferStage++;
}



