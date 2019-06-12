#include "Reorder.h"

Reorder::Reorder(boost::shared_ptr<XGpuBuffers> xGpuBuffer):xGpuBuffer(xGpuBuffer){

}

void Reorder::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    if(inPacket->isEOS()){
        std::cout <<"Reorder Class: End of stream" << std::endl;
        std::get<0>(op).try_put(inPacket);//(std::make_shared<StreamObject>());
    }else{
        boost::shared_ptr<BufferPacket> inPacket_cast = boost::dynamic_pointer_cast<BufferPacket>(inPacket); 
        boost::shared_ptr<ReorderPacket> outPacket = boost::make_shared<ReorderPacket>(inPacket->getTimestamp(),false,inPacket->getFrequency(),xGpuBuffer);
     
        #ifdef DP4A
        std::cout << "Built with DP4A, DP4A not yet implemented" << std::endl;
        throw "Built with DP4A, DP4A not yet implemented";
        #else
        for (size_t fengId = 0; fengId < NUM_ANTENNAS; fengId++)
        {
            if(inPacket_cast->isPresent(fengId)){
                DualPollComplex_in * inputArray = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId);
                for(int channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
                {
                    for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
                    {
                        DualPollComplex_in * inputSample = &inputArray[channel_index*NUM_TIME_SAMPLES + time_index];
                        //std::cout << (int)inputSample->imagPol0 << " " << (int)inputSample->imagPol1 << " " << (int)inputSample->realPol0 << " " << (int)inputSample->realPol1 << " " << std::endl;
                        ((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId] = *inputSample;
                        //std::cout << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol1 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol1 << " " << std::endl;
                    }
                }
            }else{
                DualPollComplex_in inputSample = {0,0,0,0};
                for(int channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
                {
                    for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
                    {
                        //std::cout << (int)inputSample->imagPol0 << " " << (int)inputSample->imagPol1 << " " << (int)inputSample->realPol0 << " " << (int)inputSample->realPol1 << " " << std::endl;
                        ((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId] = inputSample;
                        //std::cout << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].imagPol1 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol0 << " " << (int)((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId].realPol1 << " " << std::endl;
                    }
                }   
            }
        }
        #endif
        //std::cout << std::hex <<outPacket->getTimestamp() << std::endl;
        
        if(!std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(outPacket))){
            //std::cout << "Packet failed to be passed to GPU class" << std::endl;
        }
    }
    pipelineCounts.ReorderStage++;
}