#include "GPUWrapper.h"

GPUWrapper::GPUWrapper(boost::shared_ptr<XGpuBuffers> xGpuBuffer): accumulationsThreshold(1600),numAccumulations(0),xGpuBuffer(xGpuBuffer),storageQueue(),inputOrderedQueue(){
    oldest_timestamp = 0;
}

void GPUWrapper::setAccumulationsThreshold(int accumulationsThreshold){
    if(accumulationsThreshold>DEFAULT_ACCUMULATIONS_THRESHOLD){
        std::cout << accumulationsThreshold << " is higher than the maximum of " << DEFAULT_ACCUMULATIONS_THRESHOLD << ". Setting Threshold to the maximum" << std::endl;
    }else{
        this->accumulationsThreshold=accumulationsThreshold;
    }
}

int GPUWrapper::getAccumulationsThreshold(){
    return this->accumulationsThreshold;
}

void GPUWrapper::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){

    int64_t timestamp_diff = ((int64_t)(inPacket->getTimestamp() - oldest_timestamp)/TIMESTAMP_JUMP);

    //std::cout  << inPacket->getTimestamp() << std::endl;
    if(timestamp_diff > RESYNC_LIMIT || timestamp_diff < -RESYNC_LIMIT){
        std::cout << "Timestamp off by "<<timestamp_diff<<" samples, resync triggered in GPUWrapper class" << std::endl;
        while(inputOrderedQueue.size() > 0){
            inputOrderedQueue.pop();
        }
        //std::cout<< "new " << inPacket->getTimestamp() << std::endl;
        //std::cout<< "old " << oldest_timestamp << std::endl;
        //std::cout<<timestamp_diff<<std::endl;
        //std::cout << inputOrderedQueue.size() << std::endl;
        //numAccumulations=0;
    }

    oldest_timestamp = inPacket->getTimestamp() ;

    inputOrderedQueue.push(inPacket);
    if(inputOrderedQueue.size() > MINIMUM_QUEUE_SIZE){
        boost::shared_ptr<StreamObject> inPacketFromQueue = inputOrderedQueue.top();
        inputOrderedQueue.pop();
        //std::cout<<inPacketFromQueue->getTimestamp() << " " << inPacket->getTimestamp() << std::endl;
        //std::cout << "====== " << inPacketFromQueue->getTimestamp() << " ====" <<std::endl;
        
        if(inPacketFromQueue->isEOS()){
            std::cout <<"GPUWrapper Class: End of stream" << std::endl;
            std::get<0>(op).try_put(inPacketFromQueue);//(std::make_shared<StreamObject>());
        }else{
            boost::shared_ptr<ReorderPacket> inPacket_cast = boost::dynamic_pointer_cast<ReorderPacket>(inPacketFromQueue);
            int xgpu_error = 0;
            XGPUContext * context = xGpuBuffer->getXGpuContext_p();

            if(numAccumulations==0){
                tempGpuWrapperPacket = boost::make_shared<GPUWrapperPacket>(inPacket_cast->getTimestamp(),false,inPacket_cast->getFrequency(),xGpuBuffer);
            }
            
            context->input_offset = inPacket_cast->getBufferOffset()*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS*NUM_TIME_SAMPLES;
            context->output_offset = tempGpuWrapperPacket->getBufferOffset()*NUM_CHANNELS_PER_XENGINE*NUM_BASELINES*2*2;//2 For the real/imaginary components and 2 for the 4 products per baseline
            //std::cout << tempGpuWrapperPacket->getBufferOffset() << std::endl;

            if(numAccumulations == accumulationsThreshold-1){
                xgpu_error = xgpuCudaXengine(context,finalSyncOp);
                if(xgpu_error) {
                    std::cout << "xgpuCudaXengine returned error code: " << xgpu_error << std::endl;
                }       
                xgpu_error =xgpuClearDeviceIntegrationBuffer(context);
                if(xgpu_error) {
                    std::cout << "xgpuClearDeviceIntegrationBuffer returned error code: " << xgpu_error << std::endl;
                }
                numAccumulations=0;
                storageQueue = std::queue<boost::shared_ptr<ReorderPacket>>();

                //displayBaseline((BaselineProductsStruct_out*)context->matrix_h,17,61);
                //std::cout << std::endl;
                //std::cout << tempGpuWrapperPacket->getBufferOffset() << std::endl;
                //std::cout << std::hex << tempGpuWrapperPacket->getTimestamp() << std::endl;
                if(!std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(tempGpuWrapperPacket))){
                    std::cout << "Packet failed to be passed to SpeadTx class" << std::endl;
                }
            }else{
                xgpu_error = xgpuCudaXengine(context,syncOp);
                numAccumulations++;
                storageQueue.push(inPacket_cast);
            }
            //boost::shared_ptr<ReorderPacket> outPacket = boost::make_shared<ReorderPacket>(inPacket->getTimestamp(),false,inPacket->getFrequency());
            //if(!std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(outPacket))){
            //   std::cout << "Packet failed to be passed to GPU class" << std::endl;
            //}   
        }  
    }
    pipelineCounts.GPUWRapperStage++;
}