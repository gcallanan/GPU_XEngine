#include "GPUWrapper.h"

GPUWrapper::GPUWrapper(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager,int64_t syncStart): accumulationsThreshold(408),numAccumulations(0),xGpuBufferManager(xGpuBufferManager),storageQueue(),syncStart(syncStart * FFT_SIZE * NUM_TIME_SAMPLES * 2),synced(false),syncTimestamp(0){
    this->stageName = "GPUWrapper";
    this->armortiserMaxSize = 1;
    oldest_timestamp = 0;
}

GPUWrapper::GPUWrapper(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager, int64_t syncStart ,int accumulationsThreshold): accumulationsThreshold(accumulationsThreshold),numAccumulations(0),xGpuBufferManager(xGpuBufferManager),storageQueue(),syncStart(syncStart * FFT_SIZE * NUM_TIME_SAMPLES * 2),synced(false),syncTimestamp(0){
    this->stageName = "GPUWrapper";
    this->armortiserMaxSize = 1;
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

OutputPacketQueuePtr GPUWrapper::processPacket(boost::shared_ptr<PipelinePacket> inPacket){
    OutputPacketQueuePtr outPacketQueue = boost::make_shared<OutputPacketQueue>();
    boost::shared_ptr<PipelinePacket> inPacket_pop = inPacket;
    int64_t timestamp_diff = ((int64_t)(inPacket_pop->getTimestamp() - oldest_timestamp)/TIMESTAMP_JUMP);

    oldest_timestamp = inPacket_pop->getTimestamp() ;    

    boost::shared_ptr<TransposePacket> inPacket_cast = boost::dynamic_pointer_cast<TransposePacket>(inPacket_pop);
    int xgpu_error = 0;
    XGPUContext * context = xGpuBufferManager->getXGpuContext_p();

    //Sync Logic Start
    if(numAccumulations==0 && !synced){
        
        if(syncTimestamp == 0){
            //std::cout << "a" << std::endl;
            int64_t accLen = accumulationsThreshold * FFT_SIZE * NUM_TIME_SAMPLES * 2;
            //std::cout << "a" << std::endl;
            int64_t multiple = (inPacket_pop->getTimestamp() - syncStart)/accLen;
            //std::cout << "a" << std::endl;
            syncTimestamp = syncStart + (multiple+1)*accLen;
            //std::cout << " " << accLen << " " << multiple  << std::endl;
        }

        //std::cout << " " << syncStart << " " << syncTimestamp << " " << inPacket_pop->getTimestamp() << std::endl;
        if(syncTimestamp == inPacket_pop->getTimestamp()){
            synced = true;
            std::cout << "Succesfully Synced X-Engine!" << std::endl;
        }

        if(syncStart==0){
            synced=true;
            std::cout << "X-Engine running without a sync" << std::endl;
        }
    }
    //Sync Logic End

    if(numAccumulations==0 && synced){
        tempGpuWrapperPacket = boost::make_shared<GPUWrapperPacket>(inPacket_cast->getTimestamp(),false,inPacket_cast->getFrequency(),xGpuBufferManager);
    }
    
    if(synced){
        context->input_offset = inPacket_cast->getBufferOffset()*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS*NUM_TIME_SAMPLES;
        context->input_offset = context->input_offset + ((ALIGNMENT_BOUNDARY - ((uint64_t)context->array_h))%ALIGNMENT_BOUNDARY)/2;//I expect the 24 to be ((ALIGNMENT_BOUNDARY - ((uint64_t)context->array_h))%ALIGNMENT_BOUNDARY)/4 which equates to 12, instead i get 24. I do not know why this is twice what i expect? This is hardcoded for now;;
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
            storageQueue = std::queue<boost::shared_ptr<TransposePacket>>();

            //displayBaseline((BaselineProductsStruct_out*)context->matrix_h,17,61);
            outPacketQueue->push_back(tempGpuWrapperPacket);
        }else{
            xgpu_error = xgpuCudaXengine(context,syncOp);
            numAccumulations++;
            storageQueue.push(inPacket_cast);
        }
    }

    pipelineCounts.GPUWRapperStage++;
    return outPacketQueue;
}

