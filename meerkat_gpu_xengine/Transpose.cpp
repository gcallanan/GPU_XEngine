#include "Transpose.h"
#include <emmintrin.h>
#include <immintrin.h>

std::atomic<int> timeSincePacketsLastMissing; 

#define BLOCK_SIZE 8//Must be a power of 2
/** Use SSE instructions, can either be 1 or 0, SSE instructions greatly increase performance. Block size must be either 4,8 or 16 when SSE is 1. Ensure that your processor supports the instructions. Most processors will support a block size of 4 or 8(SSE or AVX instructions), only newer processors support 16(AVX512 instructions)*/
#define USE_SSE 1
#define USE_SSE_INPUT 0

Transpose::Transpose(boost::shared_ptr<XGpuBufferManager> xGpuBufferManager,int stageIndex):xGpuBufferManager(xGpuBufferManager),stageIndex(stageIndex){
    this->stageName = "Transpose " + std::to_string(stageIndex);
    this->armortiserMaxSize = ARMORTISER_TO_GPU_SIZE;
    timeSincePacketsLastMissing=0;
    #if  BLOCK_SIZE > (NUM_ANTENNAS/NUM_TRANSPOSE_STAGES)
        #error BLOCK_SIZE needs to be <= (NUM_ANTENNAS/NUM_TRANSPOSE_STAGES)
    #endif
}

OutputPacketQueuePtr Transpose::processPacket(boost::shared_ptr<PipelinePacket> inPacket){
    OutputPacketQueuePtr outPacketQueue = boost::make_shared<OutputPacketQueue>();
    boost::shared_ptr<BufferPacket> inPacket_cast ;
    boost::shared_ptr<TransposePacket> outPacket; 
    if(stageIndex==0){
        inPacket_cast = boost::dynamic_pointer_cast<BufferPacket>(inPacket);
        outPacket = boost::make_shared<TransposePacket>(inPacket->getTimestamp(),false,inPacket->getFrequency(),xGpuBufferManager,inPacket_cast);
        if(inPacket_cast->numPacketsReceived() != 64){
            timeSincePacketsLastMissing = 0;
        }else{
            timeSincePacketsLastMissing++;
        }
    }else{
        outPacket = boost::dynamic_pointer_cast<TransposePacket>(inPacket);
        inPacket_cast = outPacket->getInputData_ptr();
    }
    #ifdef DP4A
        #error Built with DP4A flag set, DP4A not yet implemented
    #else
    DualPollComplex_in inputSample = {0,0,0,0};
    
    for (size_t fengId = (NUM_ANTENNAS/NUM_TRANSPOSE_STAGES)*(stageIndex); fengId < (NUM_ANTENNAS/NUM_TRANSPOSE_STAGES)*(stageIndex+1); fengId+=BLOCK_SIZE)
    {
        #if USE_SSE_INPUT == 0
        DualPollComplex_in * inputArray[BLOCK_SIZE];
        #endif
        bool packetPresent[BLOCK_SIZE];
        int32_t toTransfer[BLOCK_SIZE];

        #if USE_SSE_INPUT == 0
        for (size_t block_i = 0; block_i < BLOCK_SIZE; block_i++)
        {
            inputArray[block_i] = (DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+block_i);
        }
        #endif
        for (size_t block_i = 0; block_i < BLOCK_SIZE; block_i++)
        {
            packetPresent[block_i] = inPacket_cast->isPresent(fengId+block_i);
        }

        for(int channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
        {
            #if USE_SSE_INPUT == 0
            for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
            #else
            for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index+=BLOCK_SIZE)
            #endif
            {
                #if USE_SSE_INPUT == 0
                DualPollComplex_in* dest_ptr = &(((DualPollComplex_in*) outPacket->getDataPointer())[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId]);
                #endif
                #if USE_SSE == 1
                    #if BLOCK_SIZE == 16
                        volatile __m512 reg;// = _mm512_setzero_pd();
                        for (size_t block_i = 0; block_i < 4; block_i++)
                        {   
                            volatile __m128i reg_temp = _mm_setzero_si128();
                            for(size_t avx_word = 0; avx_word < 4; avx_word++){
                                if(packetPresent[block_i*4+avx_word]){
                                    reg_temp = _mm_insert_epi32(reg_temp,*((int32_t*) &inputArray[block_i*4+avx_word][channel_index*NUM_TIME_SAMPLES + time_index]),avx_word);
                                    //reg_temp = _mm_insert_epi32(reg_temp,1289,avx_word);
                                }
                            }
                            reg = _mm512_insertf32x4(reg,(__m128)reg_temp,block_i);
                        }
                        _mm512_store_ps((__m512*)dest_ptr,reg);

                    #elif BLOCK_SIZE == 8
                        #if USE_SSE_INPUT == 0
                            __m256i reg = _mm256_setzero_si256();
                            for (size_t block_i = 0; block_i < BLOCK_SIZE; block_i++)
                            {
                                if(packetPresent[block_i]){
                                    int * inputSampleAddress = ((int32_t*) &inputArray[block_i][channel_index*NUM_TIME_SAMPLES + time_index]);
                                    reg = _mm256_insert_epi32(reg,*inputSampleAddress,block_i);
                                }
                            }
                            //_mm256_stream_si256((__m256i*)dest_ptr,reg);
                            //_mm256_storeu_si256((__m256i*)dest_ptr,reg);
                            _mm256_store_si256((__m256i*)dest_ptr,reg);
                        #else
                            //std::cout<< "F:" << fengId <<" T:"<< time_index << std::endl;
                            __m256i reg_in[BLOCK_SIZE];
                            for (size_t block_feng_index = 0; block_feng_index < BLOCK_SIZE; block_feng_index++)
                            {
                                if(packetPresent[block_feng_index]){
                                    __m256i * ptr =  (__m256i*) &(((DualPollComplex_in*)inPacket_cast->getDataPtr(fengId+block_feng_index))[channel_index*NUM_TIME_SAMPLES + time_index]);
                                    reg_in[block_feng_index] = _mm256_load_si256(ptr);
                                }else{
                                    reg_in[block_feng_index] = _mm256_setzero_si256();
                                }
                            }

                            for (size_t block_time_index = 0; block_time_index < BLOCK_SIZE; block_time_index++)
                            {
                                __m256i reg_out = _mm256_setzero_si256();
                                DualPollComplex_in* dest_ptr = &(((DualPollComplex_in*) outPacket->getDataPointer())[(time_index+block_time_index)*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId]);
                                //std::cout <<  << std::endl;
                                for (size_t block_feng_index = 0; block_feng_index < BLOCK_SIZE; block_feng_index++){
                                    __m256i tmpReg = _mm256_maskz_slli_epi32((1<<block_feng_index),reg_in[block_feng_index],block_feng_index);
                                    reg_out = _mm256_or_si256(reg_out,tmpReg);
                                }
                                _mm256_store_si256((__m256i*)dest_ptr,reg_out);
                            }
                        #endif
                    #elif BLOCK_SIZE == 4
                        __m128i reg = _mm_setzero_si128();
                        for (size_t block_i = 0; block_i < BLOCK_SIZE; block_i++)
                        {
                            if(packetPresent[block_i]){
                                reg = _mm_insert_epi32(reg,*((int32_t*) &inputArray[block_i][channel_index*NUM_TIME_SAMPLES + time_index]),block_i);
                            }
                        }
                        _mm_store_si128((__m128i*)dest_ptr,reg);
                    #else
                        #error "Wrong BLocksize for SSE instructions"
                    #endif
                #elif USE_SSE == 0
                    for (size_t block_i = 0; block_i < BLOCK_SIZE; block_i++)
                    {
                        if(packetPresent[block_i]){
                            toTransfer[block_i] = *((int32_t*) &inputArray[block_i][channel_index*NUM_TIME_SAMPLES + time_index]);
                        }else{
                            toTransfer[block_i] = *(int32_t*)&inputSample;
                        }
                    }
                    memcpy(dest_ptr,toTransfer, sizeof(int32_t)*BLOCK_SIZE);
                #else
                    #error USE_SSE must be either 1 or 0
                #endif
            }
        }

    }
    #endif
    if(stageIndex==NUM_TRANSPOSE_STAGES-1){
        outPacket->clearInputData();
    }
    
    pipelineCounts.TransposeStage[stageIndex]++;
    outPacketQueue->push_back(outPacket);
    return outPacketQueue;
}
