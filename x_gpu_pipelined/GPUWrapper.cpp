#include "GPUWrapper.h"

GPUWrapper::GPUWrapper(boost::shared_ptr<XGpuBuffers> xGpuBuffer): accumulationsThreshold(400),numAccumulations(0),xGpuBuffer(xGpuBuffer),storageQueue(){
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

    boost::shared_ptr<Spead2RxPacketWrapper> inPacketQueue = boost::dynamic_pointer_cast<Spead2RxPacketWrapper>(inPacket);
    while(inPacketQueue->getArmortiserSize() > 0)
    {
        boost::shared_ptr<StreamObject> inPacket_pop = inPacketQueue->removePacket();
        int64_t timestamp_diff = ((int64_t)(inPacket_pop->getTimestamp() - oldest_timestamp)/TIMESTAMP_JUMP);

        oldest_timestamp = inPacket_pop->getTimestamp() ;        

        boost::shared_ptr<ReorderPacket> inPacket_cast = boost::dynamic_pointer_cast<ReorderPacket>(inPacket_pop);
        int xgpu_error = 0;
        XGPUContext * context = xGpuBuffer->getXGpuContext_p();

        if(numAccumulations==0){
            tempGpuWrapperPacket = boost::make_shared<GPUWrapperPacket>(inPacket_cast->getTimestamp(),false,inPacket_cast->getFrequency(),xGpuBuffer);
        }
        
        context->input_offset = inPacket_cast->getBufferOffset()*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS*NUM_TIME_SAMPLES;
        context->output_offset = tempGpuWrapperPacket->getBufferOffset()*NUM_CHANNELS_PER_XENGINE*NUM_BASELINES*2*2;//2 For the real/imaginary components and 2 for the 4 products per baseline

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

            if(!std::get<0>(op).try_put(boost::dynamic_pointer_cast<StreamObject>(tempGpuWrapperPacket))){
                std::cout << "Packet failed to be passed to SpeadTx class" << std::endl;
            }
        }else{
            xgpu_error = xgpuCudaXengine(context,syncOp);
            numAccumulations++;
            storageQueue.push(inPacket_cast);
        }
         
    }
    pipelineCounts.GPUWRapperStage++;
}