#include "Reorder.h"
#include "global_definitions.h"
#include <iostream>

Reorder::Reorder(): first_timestamp(0){
    for (size_t i = 0; i < BUFFER_SIZE; i++)
    {
        buffer.push_front(nullptr);
    }
}

void Reorder::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    //std::cout << "-" << std::endl;
    //std::get<0>(op).try_put(tbb::flow:continue_msg());//(std::make_shared<StreamObject>());
    if(inPacket->isEOS()){
        std::cout <<"End of FIle Reorder.cpp" << std::endl;
        std::get<0>(op).try_put(inPacket);//(std::make_shared<StreamObject>());
        //std::cout << "a" << std::endl;
    }else{
        //std::cout<<"Reorder Block Called"<<std::endl;
        boost::shared_ptr<Spead2RxPacket> inPacket_cast = boost::dynamic_pointer_cast<Spead2RxPacket>(inPacket);
        uint64_t packet_timestamp = inPacket_cast->getTimestamp();
        //Check that correct timestamp is propegated
        if(first_timestamp>packet_timestamp){
            std::cout << "Timestamp smaller than minimum received in Reorder class" << std::endl;
            throw "Timestamp smaller than minimum received in Reorder class";
        }

        uint8_t index = (packet_timestamp - first_timestamp)/TIMESTAMP_JUMP;
        //std::cout<<(int)index<<" "<<TIMESTAMP_JUMP<<" " << packet_timestamp << " " << first_timestamp << " " << buffer.size()<<std::endl;
        if(index>BUFFER_SIZE+PACKET_THRESHOLD_BEFORE_SYNC){//Packet Far Outside of Range
            //std::cout << "b" << std::endl;
            for (size_t i = 0; i < BUFFER_SIZE; i++)
            {
                buffer.push_front(nullptr);
                buffer.pop_back();
            }
            buffer[0] = (boost::make_shared<ReorderPacket>(packet_timestamp,false,inPacket->getFrequency()));
            buffer[0]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
            first_timestamp = packet_timestamp;
        }else if(index>=BUFFER_SIZE){//Packet just outside of range
            //std::cout << "c" << std::endl;
            int numPops = 0;
            while((index>=BUFFER_SIZE || buffer[0] == nullptr) && numPops != BUFFER_SIZE){
                if(buffer[0] != nullptr){
                    std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(buffer[0]));
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
                buffer[0] = (boost::make_shared<ReorderPacket>(packet_timestamp,false,inPacket->getFrequency()));
                buffer[0]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                first_timestamp = buffer[0]->getTimestamp();
            }else{
                buffer[index] = nullptr;
                //std::cout << "fasdbas " << (int)index << std::endl;
                buffer[index] = (boost::make_shared<ReorderPacket>(packet_timestamp,false,inPacket->getFrequency()));
                //std::cout << "gasdbas " << (int)index << std::endl;
                //std::cout << buffer[index]->getTimestamp() << " " << buffer[index]->isEOS() << " " << buffer[index]->numPacketsReceived() << std::endl;
                buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
                //std::cout << "hasdbas " << (int)index << std::endl;
            }
        }else if(buffer[index] == nullptr){
            //std::cout << "d" << std::endl;
            buffer[index] = (boost::make_shared<ReorderPacket>(packet_timestamp,false,inPacket->getFrequency()));
            buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
        }else{
            //std::cout << "e" << std::endl;
            //std::cout << index << " " << 
            buffer[index]->addPacket(inPacket_cast->getFEngineId(),inPacket_cast->getHeapPtr(),inPacket_cast->getPayloadPtr_p());
            //std::cout << "ee" << std::endl;
            //if(index==0 && buffer[index]->numPacketsReceived() == NUM_ANTENNAS){
            //    std::cout<<"Woohooo"<<std::endl;
            //    std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(buffer[0]));
            //    buffer.pop_front();
            //    buffer.push_back(nullptr);
            //}
        }
        //std::cout << "f" << std::endl;
    }
    

    

    
}



