#include "Buffer.h"
#include "global_definitions.h"
#include <iostream>



Buffer::Buffer(): first_timestamp(0){
    for (size_t i = 0; i < BUFFER_SIZE; i++)
    {
        buffer.push_front(nullptr);
    }
}

void Buffer::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    //std::cout << "-" << std::endl;
    //std::get<0>(op).try_put(tbb::flow:continue_msg());//(std::make_shared<StreamObject>());
    /*if(debug){
        //std::cout << "a" << std::endl;
    }*/
    boost::shared_ptr<Spead2RxPacketWrapper> inPacketQueue = boost::dynamic_pointer_cast<Spead2RxPacketWrapper>(inPacket);
    

    while(inPacketQueue->getArmortiserSize() > 0){
        boost::shared_ptr<StreamObject> inPacket_pop = inPacketQueue->removePacket();
        if(inPacket_pop->isEOS()){
            std::cout <<"Buffer Class: End of stream" << std::endl;
            std::get<0>(op).try_put(inPacket_pop);//(std::make_shared<StreamObject>());
            if(debug){
                //std::cout << "b" << std::endl;
            }
        }else{
            //std::cout<<"Buffer Block Called"<<std::endl;
            boost::shared_ptr<Spead2RxPacket> inPacket_cast = boost::dynamic_pointer_cast<Spead2RxPacket>(inPacket_pop);
            uint64_t packet_timestamp = inPacket_cast->getTimestamp();
            
            if(debug){
            // std::cout << "e Timestamp: " << packet_timestamp<< " First Timestamp " << first_timestamp<<std::endl;
            }
            //Check that correct timestamp is propegated

            if((packet_timestamp - first_timestamp)/TIMESTAMP_JUMP > RESYNC_LIMIT){
                std::cout << "Timestamp completly off, resync triggered in Buffer class" << std::endl;
                first_timestamp = 0;
            }

            if(first_timestamp>packet_timestamp){
                std::cout << "Timestamp smaller than minimum received in Buffer class" << std::endl;
                throw "Timestamp smaller than minimum received in Buffer class";
            }

            int index = (packet_timestamp - first_timestamp)/TIMESTAMP_JUMP;
            //if(inPacket_cast->getFEngineId() == 0){
            //    std::cout << std::hex <<  packet_timestamp << " " << index <<std::endl;;
            //}
            //std::cout<<(int)index<<" "<<TIMESTAMP_JUMP<<" " << packet_timestamp << " " << first_timestamp << " " << buffer.size()<<std::endl;
            if(index>BUFFER_SIZE+PACKET_THRESHOLD_BEFORE_SYNC){//Packet Far Outside of Range
                //std::cout << "b" << std::endl;
                for (size_t i = 0; i < BUFFER_SIZE; i++)
                {
                    buffer.push_front(nullptr);
                    buffer.pop_back();
                }
                buffer[0] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                buffer[0]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                first_timestamp = packet_timestamp;
            }else if(index>=BUFFER_SIZE){//Packet just outside of range
                //std::cout << "c" << std::endl;
                int numPops = 0;
                while((index>=BUFFER_SIZE || buffer[0] == nullptr) && numPops != BUFFER_SIZE){
                    if(buffer[0] != nullptr){
                        if(debug){
                            //std::cout << "c" << std::endl;
                        }
                        if(!std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(buffer[0]))){
                        //    std::cout << "Packet Failed to be passed to reorder class" << std::endl;
                        }
                    }
                    buffer.pop_front();
                    //std::cout << "basdbas" << buffer.size()<<std::endl;
                    buffer.push_back(nullptr);
                    //std::cout << "basdbas" << buffer.size()<<std::endl;
                    if(buffer[0]!=nullptr){
                        first_timestamp = buffer[0]->getTimestamp();
                        index = (packet_timestamp - first_timestamp)/TIMESTAMP_JUMP;
                    }
                    numPops++;
                    //std::cout << "casdcas" << std::endl;
                }
                //std::cout << "dasdbas " << (int)index << std::endl;
                if(numPops == BUFFER_SIZE){
                    //std::cout << "iasdbas " << (int)index << std::endl;
                    buffer[0] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                    buffer[0]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                    first_timestamp = buffer[0]->getTimestamp();
                }else if(numPops > BUFFER_SIZE){
                    std::cout << "Buffer Class numPops > BUFFER_SIZE" << std::endl;
                }else{
                    buffer[index] = nullptr;
                    
                    buffer[index] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                    //std::cout << "gasdbas " << (int)index << std::endl;
                    //std::cout << buffer[index]->getTimestamp() << " " << buffer[index]->isEOS() << " " << buffer[index]->numPacketsReceived() << std::endl;
                    buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                    //std::cout << "hasdbas " << (int)index << std::endl;
                }
            }else if(buffer[index] == nullptr){
                //std::cout << "d" << std::endl;
                buffer[index] = (boost::make_shared<BufferPacket>(packet_timestamp,false,inPacket->getFrequency()));
                buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
            }else{
                buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
            } 
        }
    }
    pipelineCounts.BufferStage++;
    if(debug){
        //std::cout << "z" << std::endl;
    }


    
}



