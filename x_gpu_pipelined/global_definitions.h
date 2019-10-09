/**
 * \file global_definitions.h
 *
 * \brief File containing most user configurable parameters
 *
 * \author Gareth Callanan
 * 
 * This file contains most of the macros that the user will need to change for their own needs. It also contains the pipeline packet classes that are used to pass data between pipeline stages. The 
 *
 */

#ifndef _GLOBAL_DEFINITIONS_H
#define _GLOBAL_DEFINITIONS_H

#include <cstdint>
#include "tbb/flow_graph.h"
#include <boost/asio.hpp>
#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#include <spead2/recv_udp_pcap.h>
#include <spead2/recv_heap.h>
#include <spead2/recv_live_heap.h>
#include <spead2/recv_ring_stream.h>
#include <spead2/recv_stream.h>
#include <bitset>
#include <map>
#include <mutex>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "xgpu.h"
#include <atomic>

//Global Defines
/** Number of antennas in the array. */
#define NUM_ANTENNAS 64
/** Number of channels to be processed per X-Engine. Also the number of channels per SPEAD heap */
#define NUM_CHANNELS_PER_XENGINE 16 
/** Size of the FFT performed by the F-Engines.*/
/** Number of polarisations per channel to be processed per X-Engine */
#define NUM_POLLS 2
/** Number of time samples per SPEAD heap */
#define NUM_TIME_SAMPLES 256


#define FFT_SIZE (NUM_CHANNELS_PER_XENGINE*4*NUM_ANTENNAS)
/** Size of a quadrant - This is used for displaying the baseline as the ordering does not follow a simple pattern. See displayBaseline() and getBaselineOffset() for an implementation of baseline ordering */
#define QUADRANT_SIZE ((NUM_ANTENNAS/2+1)*(NUM_ANTENNAS/4))
/** Number of baselines being output */
#define NUM_BASELINES (QUADRANT_SIZE*4)


/** Number of  consecutive timestamps the Buffer class will have room to buffer */
#define BUFFER_SIZE 40

/** If the incoming packet has a timestamp which is greater than RESYNC_LIMIT then a resync is triggered */
#define RESYNC_LIMIT 200

/** Difference in time between consecutive timestamps */
#define TIMESTAMP_JUMP (NUM_TIME_SAMPLES*2*FFT_SIZE)

/** Maximum number of accumulations */
#define DEFAULT_ACCUMULATIONS_THRESHOLD ((int)(1632*1.5))

/** Before packets are handed over to the next stage of the pipeline they are grouped into a larger packet to reduce thread overhead. ARMORTISER_SIZE specifies the number of packets to group*/
#define ARMORTISER_SIZE 100

/** Same function as ARMORTISER_SIZE but specifically implemented for the transmission from the Transpose to the GPUWrapper class. This class needs to have a smaller ARMORTISER_SIZE as the packets q ueued here are holding GPU memory which we want to use efficiently */
#define ARMORTISER_TO_GPU_SIZE 10

//Transpose 
/** Number of stages for the reoder pipeline module. Each stage is processed by a different thread */
#define NUM_TRANSPOSE_STAGES 2
/** Block size to perform the transpose, must be a power of two.*/
#define BLOCK_SIZE 8//Must be a power of 2
/** Use SSE instructions, can either be 1 or 0, SSE instructions greatly increase performance. Block size must be either 4,8 or 16 when SSE is 1. Ensure that your processor supports the instructions. Most processors will support a block size of 4 or 8(SSE or AVX instructions), only newer processors support 16(AVX512 instructions)*/
#define USE_SSE 1

//Global Structs

/**
 * Struct counters for different stages in the pipeline. This is used to monitor flow rates through the pipeline
 */
typedef struct PipelineCountsStruct{
   std::atomic<int> Spead2RxStage; /**< Number of packets processed by Spead2Rx pipeline.*/
   std::atomic<int> BufferStage; /**< Number of packets processed by Buffer pipeline stage. */
   std::atomic<int> TransposeStage[NUM_TRANSPOSE_STAGES]; /**< Number of packets processed by by the Transpose pipeline class. */
   std::atomic<int> GPUWRapperStage; /**< Number of packets processed by the GPUWrapper pipeline class. */
   std::atomic<int> Spead2TxStage; /**< Number of packets processed by SpeadTx class. */
   std::atomic<int> heapsDropped; /**< Number of heaps dropped due to not receiving all ethernet packets. */
   std::atomic<int> heapsReceived; /**< Total number of heaps received.(Complete and dropped) */
   std::atomic<int> packetsTooLate; /**< Packets received by Buffer class that were dropped as the timestamp is too late to arrive */
} PipelineCounts;

/**
 * Struct storing dual pol complex samples as received by the F-Engines
 */
typedef struct DualPollComplexStruct_in {
  int8_t realPol0;/**< Pol 0 Real Memeber. */
  int8_t imagPol0;/**< Pol 0 Imaginary Memeber. */
  int8_t realPol1;/**< Pol 1 Real Memeber. */
  int8_t imagPol1;/**< Pol 1 Imaginary Memeber. */
} DualPollComplex_in;

/**
 * Strut contatining baseline products for a pair of antennas. For each antenna pair, two of these structs are required, one for the imaginary component and the other for the real component
 */
#ifndef DP4A
typedef struct BaselineProductsStruct_out {
  float product0;/**< <Antenna x, Antenna x> product. */
  float product1;/**< <Antenna x, Antenna y> product. */
  float product2;/**< <Antenna y, Antenna x> product. */
  float product3;/**< <Antenna y, Antenna y> product. */
} BaselineProducts_out;
#else
typedef struct BaselineProductsStruct_out {
  int32_t product0;/**< <Antenna x, Antenna x> product. */
  int32_t product1;/**< <Antenna x, Antenna y> product. */
  int32_t product2;/**< <Antenna y, Antenna x> product. */
  int32_t product3;/**< <Antenna y, Antenna y> product. */
} BaselineProducts_out;
#endif

/**
 * Struct pointing to location in pinned host memory that will be transferred to the GPU. The XGpuBuffers class manages the allocation of this memory, the XGpuInputBufferPacket just stores the pointer and index of this in the buffer 
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
 * \brief Function to get the index of a single baseline in a packet of baselines output from xGPU. 
 * This offset is not in bytes, but in baselines. It needs to be multiplied by size of a baseline to search through bytes.
 * It is required that ant0>ant1 or else an error is thrown
 * \param[in] XGpuPacketOut Pointer to packet containing the output baselines
 * \param[in] ant0 Index of first antenna.
 * \param[in] ant1 Index of second antenna
 */
int getBaselineOffset(int ant0, int ant1);

/** 
 * \brief Function to print out an individual baseline.
 * It is required that i>j or else an error is thrown
 * \param[in] XGpuPacketOut Pointer to packet containing the output baselines
 * \param[in] i Index of first antenna.
 * \param[in] j Index of second antenna
 */
void displayBaseline(BaselineProducts_out* XGpuPacketOut, int i, int j);

/** Number of output xGPU packets to allocate space for. This needs to be greater than 1 so that another output packet can be allocated while another is being processed. However this does not need to be too large as a new packet gets created in the order of seconds.*/
#define DEFAULT_XGPU_OUTPUT_BUFFERS_THRESHOLD 4

/**
 * \brief A class for managing xGPU initialisation and memory assignment.
 * 
 * It will allocate memory when required as well as handle the freeing of this memory. This is required as xGPU has a fixed input buffer that is pinned to memory. No new memory can be assigned. This is implemented as a ring buffer.
 * \author Gareth Callanan
 */
class XGpuBuffers{
    public:
        /**
         * Default Constructor for XGpuBuffer. 
         * 
         * This constructor creates the input and output ring buffers for xGPU. 
         * 
         * It also initialised the xgpuContext object which is required by the xGPU library
         */
        XGpuBuffers(): lockedLocations_CpuToGpu(0),mutex_array_CpuToGpu(new std::mutex[DEFAULT_ACCUMULATIONS_THRESHOLD]),mutex_array_GpuToCpu(new std::mutex[DEFAULT_XGPU_OUTPUT_BUFFERS_THRESHOLD]){
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


/**
 * \brief A base class for packets to be passed between stages in the pipline.
 * 
 * This base class does not containe any stage specific variables. All packets passed on the pipeline will have this class as their parent
 * 
 * \author Gareth Callanan
 */
class PipelinePacket{
    public:
        /** 
         * \brief Constructor for standard Stream Object packet
         * 
         * \param eos True for End of Stream(EOS), false otherwise
         * \param timestamp_u64 Packet Timestamp
         * \param frequency Base Frequency of packet. Frequency in packet ranges from frequency to frequency+NUM_CHANNELS_PER_XENGINE
         */
        PipelinePacket(uint64_t timestamp_u64,bool eos,uint64_t frequency): timestamp_u64(timestamp_u64),eos(eos),frequency(frequency){

        }

        /** 
         * \brief Constructor for empty packet
         * 
         * Creates an empty packet with the option to set the End of Stream(EOS) flag
         * 
         * \param eos True for EOS, false otherwise
         */
        PipelinePacket(bool eos): eos(eos),timestamp_u64(0),frequency(0){

        }

        virtual uint64_t getTimestamp(){
          return timestamp_u64;
        }
        uint64_t getFrequency(){
          return frequency;
        }

        /**
         * \brief Determines if this is an End Of Stream packet
         * 
         * \return True for End of Stream, False Otherwise
         */
        bool isEOS(){
          return eos;
        }


        /**
         * \brief Comparator for < operator
         * 
         * Compares packets according to timestamps
         */
        friend bool operator<(PipelinePacket& lhs, PipelinePacket& rhs)
        {
          return lhs.getTimestamp() < rhs.getTimestamp();
        }

        /**
         * \brief Comparator for > operator
         * 
         * Compares packets according to timestamps
         */
        friend bool operator>(PipelinePacket& lhs, PipelinePacket& rhs)
        {
          return lhs.getTimestamp() > rhs.getTimestamp();
        }
    protected:
        uint64_t timestamp_u64;
        const bool eos;
        const uint64_t frequency;
};

/**
 * \brief Packet that is transmitted by the Spead2Rx class
 * 
 * Contains a pointer to a single SPEAD heap. This heap has an associated F-Engine ID on top of the other general PipelinePacket parameters
 * 
 * \author Gareth Callanan
 */
class Spead2RxPacket: public PipelinePacket
{
    public:
        /** 
         * \brief Constructor for standard Stream Object packet
         * 
         * \param eos True for End of Stream(EOS), false otherwise
         * \param timestamp_u64 Packet Timestamp
         * \param frequency Base Frequency of packet. Frequency in packet ranges from frequency to frequency+NUM_CHANNELS_PER_XENGINE
         * \param fheap Pointer to the heap received from a SPEAD2 stream
         * \param payloadPtr_p Pointer to the payload of the heap. This prevents this value having to be re-calculated.
         * \param fEngineId The ID of the F-Engine that this packet is from
         */
        Spead2RxPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,uint64_t fEngineId,uint8_t *payloadPtr_p, boost::shared_ptr<spead2::recv::heap>fheap): PipelinePacket(timestamp_u64,eos,frequency),fEngineId(fEngineId),fheap(fheap),payloadPtr_p(payloadPtr_p){

        }
        uint64_t getFEngineId(){
          return fEngineId;
        }
        boost::shared_ptr<spead2::recv::heap> getHeapPtr(){
          return fheap;
        }
        uint8_t * getPayloadPtr_p(){
          return payloadPtr_p;
        }
        
    private:
        uint64_t fEngineId;
        uint8_t * payloadPtr_p;
        boost::shared_ptr<spead2::recv::heap> fheap;

};

class BufferPacket: public virtual PipelinePacket{
    public:
        BufferPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency) : PipelinePacket(timestamp_u64,eos,frequency),fEnginesPresent_u64(0),numFenginePacketsProcessed(0),heaps_v(){
            heaps_v.clear();
            heaps_v.reserve(NUM_ANTENNAS);
            data_pointers_v.clear();
            data_pointers_v.reserve(NUM_ANTENNAS);
            for (size_t i = 0; i < NUM_ANTENNAS; i++)
            {
              heaps_v.push_back(nullptr);
              data_pointers_v.push_back(nullptr);
            }
            
        }
        
        void addPacket(int antIndex,boost::shared_ptr<spead2::recv::heap> fheap,uint8_t* data_ptr){
            if(!this->isPresent(antIndex)){
              heaps_v[antIndex]=fheap;
              data_pointers_v[antIndex] = data_ptr;
              numFenginePacketsProcessed++;
              fEnginesPresent_u64 |= 1UL << antIndex;
            }else{
              std::cout << "Received a duplicate packet" << std::endl;
              throw "Received a duplicate packet";
            }
        }

        bool isPresent(int antIndex){
            return (fEnginesPresent_u64 >> antIndex) & 1U;
        }

        int numPacketsReceived(){
          return numFenginePacketsProcessed;
        }

        uint8_t * getDataPtr(int antIndex){
          return data_pointers_v[antIndex];
        }

    private:
        uint8_t numFenginePacketsProcessed;/**< Number of F-Engine packets recieved, equal to NUM_ANTENNAS if all packets are received, missing antennas should have their data zeroed*/
        uint64_t fEnginesPresent_u64;/**< The bits in this field from 0 to NUM_ANTENNAS will be set to 1 if the corresponding F-Engine packet has been recieved or 0 otherwise.. */
        std::vector<boost::shared_ptr<spead2::recv::heap>> heaps_v;
        std::vector<uint8_t*> data_pointers_v;
};

class PacketArmortiser: public virtual PipelinePacket{
    public:
      PacketArmortiser(): PipelinePacket(false){
      }
      void addPacket(boost::shared_ptr<PipelinePacket> packetIn){
        packets.push_back(packetIn);
      }
      boost::shared_ptr<PipelinePacket> removePacket(){
        boost::shared_ptr<PipelinePacket> outPacket = packets.front();
        packets.pop_front();
        return outPacket;
      }
      int getArmortiserSize(){
        return packets.size();
      }
    private:
      std::deque<boost::shared_ptr<PipelinePacket>> packets;

};

class TransposePacket: public virtual PipelinePacket{
    public:
        TransposePacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,boost::shared_ptr<XGpuBuffers> xGpuBuffer,boost::shared_ptr<BufferPacket> inputData_p): PipelinePacket(timestamp_u64,eos,frequency),xGpuBuffer(xGpuBuffer),packetData(xGpuBuffer->allocateMemory_CpuToGpu()),inputData_p(inputData_p){
        
        }
        uint8_t * getDataPointer(){
            return packetData.data_ptr; 
        }
        int getBufferOffset(){
            return packetData.offset;
        }
        boost::shared_ptr<BufferPacket> getInputData_ptr(){
          return inputData_p;
        }
        void clearInputData(){
          inputData_p = nullptr;
        }
        ~TransposePacket(){
          xGpuBuffer->freeMemory_CpuToGpu(packetData.offset);
          //std::cout<<"Deleted"<<std::endl;
        }
    private:
        XGpuInputBufferPacket packetData; 
        boost::shared_ptr<XGpuBuffers> xGpuBuffer;
        boost::shared_ptr<BufferPacket> inputData_p;

};

class GPUWrapperPacket: public virtual PipelinePacket{
    public:
        GPUWrapperPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,boost::shared_ptr<XGpuBuffers> xGpuBuffer): PipelinePacket(timestamp_u64,eos,frequency),xGpuBuffer(xGpuBuffer), packetData(xGpuBuffer->allocateMemory_GpuToCpu()){
        
        }
        uint8_t * getDataPointer(){
            return packetData.data_ptr; 
        }
        int getBufferOffset(){
            return packetData.offset;
        }
        ~GPUWrapperPacket(){
          xGpuBuffer->freeMemory_GpuToCpu(packetData.offset);
        }
        
    private:
        XGpuOutputBufferPacket packetData; 
        boost::shared_ptr<XGpuBuffers> xGpuBuffer;

};


typedef tbb::flow::multifunction_node<boost::shared_ptr<PipelinePacket>, tbb::flow::tuple<boost::shared_ptr<PipelinePacket> > > multi_node;

struct PipelinePacketPointerCompare
{
    bool operator()(const boost::shared_ptr<PipelinePacket>& lhs, const boost::shared_ptr<PipelinePacket>& rhs)
    {
        return *lhs > *rhs;
    }
};


//Global Variables;
extern PipelineCounts pipelineCounts;
extern bool debug;
extern int speadTxSuccessCount;


#endif
