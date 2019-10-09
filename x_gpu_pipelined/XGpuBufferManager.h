#ifndef _XGPU_BUFFER_MANAGER_H
#define _XGPU_BUFFER_MANAGER_H

#include <stdio.h>
#include <mutex>
#include <atomic>
#include "xgpu.h"
#include "xgpu_info.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include "global_definitions.h"

/** Number of output xGPU packets to allocate space for. This needs to be greater than 1 so that another output packet can be allocated while another is being processed. However this does not need to be too large as a new packet gets created in the order of seconds.*/
#define DEFAULT_XGPU_OUTPUT_BUFFERS_THRESHOLD 4


/**
 * Struct pointing to location in pinned host memory that will be transferred to the GPU. The XGpuBufferManager class manages the allocation of this memory, the XGpuInputBufferPacket just stores the pointer and index of this in the buffer 
 * The samples are ordered from [time][channel][antenna][polarization][complexity](ordered from slowest to fastest changing values) when not using DP4A instructions.
 * \param data_ptr Pointer to location in pinned host memory buffer
 * \param offset The position of this packet within the other packets in the buffer
 */
typedef struct XGpuInputBufferPacketStruct{
    uint8_t * data_ptr;
    int offset;
} XGpuInputBufferPacket;

/**
 * Struct  pointing to the location in host memory that the data that is generated by xGPU is stored. 
 * The data is arranged as [complexity][channel][antenna][antenna](ordered from slowest to fastest changing values).
 * The function getBaselineOffset() shows the ordering of the [antenna][antenna] section. This ordering is complicated as the baselines are split into quadrants based on whether antenna1/antenna2 are odd or even 
 * \param data_ptr Pointer to location in pinned host memory buffer
 * \param offset The position of this packet within the other packets in the buffer
 */
typedef struct XGpuOutputBufferPacketStruct{
    uint8_t * data_ptr;
    int offset;
} XGpuOutputBufferPacket;

/**
 * \brief A class for managing xGPU initialisation and memory assignment.
 * 
 * It will allocate memory when required as well as handle the freeing of this memory. This is required as xGPU has a fixed input buffer that is pinned to memory. No new memory can be assigned. This is implemented as a ring buffer.
 * \author Gareth Callanan
 */
class XGpuBufferManager{
    public:
        /**
         * Default Constructor for XGpuBuffer. 
         * 
         * This constructor creates the input and output ring buffers for xGPU. 
         * 
         * It also initialised the xgpuContext object which is required by the xGPU library
         */
        XGpuBufferManager(): lockedLocations_CpuToGpu(0),mutex_array_CpuToGpu(new std::mutex[DEFAULT_ACCUMULATIONS_THRESHOLD]),mutex_array_GpuToCpu(new std::mutex[DEFAULT_XGPU_OUTPUT_BUFFERS_THRESHOLD]){
          xgpuInfo(&xgpu_info);
          int xgpu_error = 0;
          index_CpuToGpu=0;
          lockedLocations_CpuToGpu=0;
          context.array_len = xgpu_info.vecLength*DEFAULT_ACCUMULATIONS_THRESHOLD;//Determines size of input ring buffer. With xgpu_info.vecLength being the size of a single input packet and DEFAULT_ACCUMULATIONS_THRESHOLD being the total number of packets to be able to store
          context.matrix_len = xgpu_info.matLength*DEFAULT_XGPU_OUTPUT_BUFFERS_THRESHOLD;//Determines size of output ring buffer. With xgpu_info.matLength being the size of a single output packet and DEFAULT_XGPU_OUTPUT_BUFFERS_THRESHOLD being the total number of packets to be able to store
          context.array_h = (ComplexInput*)malloc(context.array_len*sizeof(ComplexInput));
          context.matrix_h = (Complex*)malloc(context.matrix_len*sizeof(Complex));
          std::cout << ((float)(context.array_len*sizeof(ComplexInput)))/1024/1024/1024 << " GB of gpu storage allocated." << std::endl; 
          xgpu_error = xgpuInit(&context, 0);
          if(xgpu_error) {
              std::cout << "xgpuInit returned error code: " << xgpu_error << std::endl;
          }
        }

        /**
         * Returns a pointer to the xGPU Context object.
         * \return Pointer to XGPUContext
         */
        XGPUContext * getXGpuContext_p(){
          return &context;
        }

        /**
         * Allocates a single input packet size worth of memory in the CpuToGpu ring buffer.
         * Blocks if there is no free space available.
         * \return Struct with details of allocated CpuToGpu memory
         */
        XGpuInputBufferPacket allocateMemory_CpuToGpu(){
          int currentIndex = index_CpuToGpu++ % DEFAULT_ACCUMULATIONS_THRESHOLD;
          mutex_array_CpuToGpu[currentIndex].lock();
          XGpuInputBufferPacket structOut;
          structOut.data_ptr = (uint8_t*)(context.array_h + xgpu_info.vecLength*currentIndex);
          structOut.offset = currentIndex;
          lockedLocations_CpuToGpu++;
          if(lockedLocations_CpuToGpu > DEFAULT_ACCUMULATIONS_THRESHOLD-10){
            std::cout << "The XGpuBuffers have been overallocated. Pipeline is stalled until they are free." << std::endl;
          }
          return structOut;
        }

        /**
         * Frees memory from the CpuToGpu ring buffer.
         * \param[in] blockToFree The index of the object to free. This index is the XGpuInputBufferPacket.offset value.
         */
        void freeMemory_CpuToGpu(int blockToFree){
          mutex_array_CpuToGpu[blockToFree].unlock();
          lockedLocations_CpuToGpu--;
        }

        /**
         * Allocates a single output packet size worth of memory in the GpuToCpu ring buffer.
         * Blocks if there is no free space available.
         * \return Struct with details of the GpuToCpu buffer
         */
        XGpuOutputBufferPacket allocateMemory_GpuToCpu(){
          int currentIndex = index_GpuToCpu++ % DEFAULT_XGPU_OUTPUT_BUFFERS_THRESHOLD;
          mutex_array_GpuToCpu[currentIndex].lock();
          XGpuOutputBufferPacket structOut;
          structOut.data_ptr = (uint8_t*)(context.matrix_h + xgpu_info.matLength *currentIndex);
          structOut.offset = currentIndex;
          lockedLocations_GpuToCpu++;
          return structOut;
        }

        /**
         * Frees memory from the GpuToCpu ring buffer
         * \param[in] blockToFree The index of the object to free. This index is the XGpuOutputBufferPacket.offset value.
         */
        void freeMemory_GpuToCpu(int blockToFree){
          mutex_array_GpuToCpu[blockToFree].unlock();
          lockedLocations_GpuToCpu--;
        }

    private:
        XGPUInfo xgpu_info;
        boost::shared_ptr<std::mutex[]> mutex_array_CpuToGpu;//Mutex for each index in the CpuToGpu ring buffer
        boost::shared_ptr<std::mutex[]> mutex_array_GpuToCpu;//Mutex for each index in the GpuToCpu ring buffer
        std::atomic<int> lockedLocations_CpuToGpu;//Tracks the number of locked input locations. This is not required for anything. Useful to track in case of a bug
        std::atomic<int> index_CpuToGpu;// Circular buffer index
        std::atomic<int> lockedLocations_GpuToCpu;//Tracks the number of locked outputlocations. This is not required for anything. Useful to track in case of a bug
        std::atomic<int> index_GpuToCpu;// Circular buffer index
        XGPUContext context;
};



#endif