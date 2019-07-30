#include "Reorder.h"
#include <emmintrin.h>

#define BLOCK_SIZE 4

std::atomic<int> timeSincePacketsLastMissing; 

Reorder::Reorder(boost::shared_ptr<XGpuBuffers> xGpuBuffer):xGpuBuffer(xGpuBuffer){
    timeSincePacketsLastMissing=0;
}

void Reorder::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    if(inPacket->isEOS()){
        std::cout <<"Reorder Class: End of stream" << std::endl;
        std::get<0>(op).try_put(inPacket);//(std::make_shared<StreamObject>());
    }else{
        boost::shared_ptr<BufferPacket> inPacket_cast = boost::dynamic_pointer_cast<BufferPacket>(inPacket); 
        boost::shared_ptr<ReorderPacket> outPacket = boost::make_shared<ReorderPacket>(inPacket->getTimestamp(),false,inPacket->getFrequency(),xGpuBuffer);
        //std::cout <<std::hex<< inPacket->getTimestamp() << std::endl;
        #ifdef DP4A
        std::cout << "Built with DP4A, DP4A not yet implemented" << std::endl;
        throw "Built with DP4A, DP4A not yet implemented";
        #else
        int accumulation_temp = 0;
        DualPollComplex_in inputSample = {0,0,0,0};
        if(inPacket_cast->numPacketsReceived() != 64){
            //std::cout << "Sets Since last Drop: " <<  timeSincePacketsLastMissing<< " Num Packets Received in set: " << inPacket_cast->numPacketsReceived() << std::endl;
            timeSincePacketsLastMissing = 0;
        }else{
            timeSincePacketsLastMissing++;
        }
        
        for (size_t fengId = 0; fengId < NUM_ANTENNAS; fengId+=BLOCK_SIZE)
        {
            DualPollComplex_in * inputArray[BLOCK_SIZE];
            bool packetPresent[BLOCK_SIZE];
            int32_t toTransfer[BLOCK_SIZE];

            for (size_t block_i = 0; block_i < BLOCK_SIZE; block_i++)
            {
                inputArray[block_i] = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+block_i);
                packetPresent[block_i] = inPacket_cast->isPresent(fengId+block_i);
            }
            
            //DualPollComplex_in * inputArray3 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+4);
            //DualPollComplex_in * inputArray4 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+5);
            //DualPollComplex_in * inputArray5 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+6);
            //DualPollComplex_in * inputArray6 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+7);
            //DualPollComplex_in * inputArray7 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+8);
            
            //bool packet3Present = inPacket_cast->isPresent(fengId+3);
            //bool packet4Present = inPacket_cast->isPresent(fengId+4);
            //bool packet5Present = inPacket_cast->isPresent(fengId+6);
            //bool packet6Present = inPacket_cast->isPresent(fengId+7);
            //bool packet7Present = inPacket_cast->isPresent(fengId+8);
            for(int channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
            {
                for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
                {

                    for (size_t block_i = 0; block_i < BLOCK_SIZE; block_i++)
                    {
                        if(packetPresent[block_i]){
                            toTransfer[block_i] = *((int32_t*) &inputArray[block_i][channel_index*NUM_TIME_SAMPLES + time_index]);
                        }else{
                            toTransfer[block_i] = *(int32_t*)&inputSample;
                        }
                    }

                    DualPollComplex_in* dest_ptr = &(((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId]);
                    #if BLOCK_SIZE == 8
                        __m256i reg = _mm256_setr_epi32(toTransfer[0],toTransfer[1],toTransfer[2],toTransfer[3],toTransfer[4],toTransfer[5],toTransfer[6],toTransfer[7]);
                        //std::cout << "Dest Pointer : " << dest_ptr << std::endl;
                        _mm256_store_si256((__m256i*)dest_ptr,reg);
                    #elif BLOCK_SIZE == 4
                        __m128i reg = _mm_setr_epi32(toTransfer[0],toTransfer[1],toTransfer[2],toTransfer[3]);
                        _mm_store_si128((__m128i*)dest_ptr,reg);
                    #endif
                    //__m256i reg = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
                    //__m128i reg = _mm_setr_epi32(toTransfer[0],toTransfer[1],toTransfer[2],toTransfer[3]);
                    
                    //DualPollComplex_in* dest_ptr2 = &(((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId+4]);
                    //*dest_ptr = *(DualPollComplex_in*)&toTransfer[0];
                    //_mm256_storeu_si128((__m128i*)dest_ptr,reg);
                    //std::cout << "asd" << std::endl;
                    
                    //std::cout << "asd" << std::endl;
                    //_mm_storeu_si128((__m128i*)dest_ptr,reg);
                }

                
            }

        }
        #endif
        
            /* if(inPacket_cast->isPresent(fengId)){
                DualPollComplex_in * inputArray0 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId);
                DualPollComplex_in * inputArray1 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+1);
                DualPollComplex_in * inputArray2 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+2);
                DualPollComplex_in * inputArray3 = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+3);
                for(int channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
                {
                    for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
                    {
                        //DualPollComplex_in * inputSample = &inputArray[channel_index*NUM_TIME_SAMPLES + time_index];
                        //std::cout << (int)inputSample->imagPol0 << " " << (int)inputSample->imagPol1 << " " << (int)inputSample->realPol0 << " " << (int)inputSample->realPol1 << " " << std::endl;
                        //accumulation_temp += *((int32_t*) &inputArray[channel_index*NUM_TIME_SAMPLES + time_index]);
                        
                        //int32_t * dest_pointer = ((int32_t*) outPacket->getDataPointer()) + time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId;
                        //int32_t * start_pointer = ((int32_t*)inputArray) + channel_index*NUM_TIME_SAMPLES + time_index;
                        //std::cout << dataToWrite << std::endl;
                        //int64_t a;
                        //_mm_stream_si32(&(((int32_t*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId]),1);
                        //*dest_pointer = *start_pointer;                  
                        //memcpy(dest_pointer,start_pointer,4);  
                        //_mm_stream_si64((long long*)&a,*start_pointer);
                        
                        //a = 12;
                        //((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId] = inputSample;
                        ((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId+0] = inputArray0[channel_index*NUM_TIME_SAMPLES + time_index];
                        //std::cout << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol1 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol1 << " " << std::endl;
                    }
                }
            }else{
                //std::cout <<std::hex<< inPacket->getTimestamp() <<std::dec<<": Missing Packet: " << fengId << std::endl;
                for(int channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
                {
                    for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
                    {
                        //accumulation_temp += 0;
                        //std::cout << (int)inputSample->imagPol0 << " " << (int)inputSample->imagPol1 << " " << (int)inputSample->realPol0 << " " << (int)inputSample->realPol1 << " " << std::endl;
                        //int32_t * dest_pointer = &(((int32_t*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId]);
                        //_mm_stream_si32(dest_pointer,0);
                        ((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId] = inputSample;
                        //std::cout << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol1 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol1 << " " << std::endl;
                    }
                }   
            }*/
        
        //std::cout << std::hex <<outPacket->getTimestamp() << std::endl;
        //*((int32_t*)outPacket->getDataPointer()) = accumulation_temp;
        if(!std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(outPacket))){
            //std::cout << "Packet failed to be passed to GPU class" << std::endl;
        }
    }
    pipelineCounts.ReorderStage++;
}