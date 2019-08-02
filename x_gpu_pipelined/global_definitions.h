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
#define NUM_ANTENNAS 64
#define NUM_CHANNELS_PER_XENGINE 16 
#define FFT_SIZE (NUM_CHANNELS_PER_XENGINE*4*NUM_ANTENNAS)
#define NUM_POLLS 2
#define NUM_TIME_SAMPLES 256
#define QUADRANT_SIZE ((NUM_ANTENNAS/2+1)*(NUM_ANTENNAS/4))
#define NUM_BASELINES (QUADRANT_SIZE*4)


//Buffer Specific Packets
#define BUFFER_SIZE 20
#define RESYNC_LIMIT 200
#define TIMESTAMP_JUMP (NUM_TIME_SAMPLES*2*FFT_SIZE)

//X Engine Specific Variables
#define DEFAULT_ACCUMULATIONS_THRESHOLD ((int)(1632*1.2))

//Reorder Specific Variables
#define ARMORTISER_SIZE 100
#define ARMORTISER_TO_GPU_SIZE 20

//Global Structs

/**
 * Struct Storing Pipeline Stages implementation 
 */
typedef struct PipelineCountsStruct{
   std::atomic<int> Spead2RxStage;
   std::atomic<int> BufferStage;
   std::atomic<int> ReorderStage;
   std::atomic<int> GPUWRapperStage;
   std::atomic<int> Spead2TxStage;
   std::atomic<int> heapsDropped;
   std::atomic<int> heapsReceived;
   std::atomic<int> packetsTooLate;
} PipelineCounts;

/**
 * Struct storing samples as formatted bythe F-Engines
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
 * @brief: Struct containting the data that will be fed into xGPU.
 * 
 * Struct containting the data that will be fed into xGPU.
 * The samples are ordered from [time][channel][station][polarization][complexity](ordered from slowest to fastest changing values) when not using DP4A instructions. 
 * 
 * The ordering is different if DP4A is required, the general ordering remains the same but the timestamps are interleaved . See function (TODO: Insert function here) for more information
 */
typedef struct XGpuPacketInStruct{
    uint64_t timestamp_u64;/**< Timestamp. */
    uint64_t fEnginesPresent_u64;/**< The bits in this field from 0 to NUM_ANTENNAS will be set to 1 if the corresponding F-Engine packet has been recieved or 0 otherwise.. */
    uint64_t frequencyBase_u64;/**< Base Frequency of the samples packet. Frequeny in packet run from frequencyBase_u64 to frequencyBase_u64+NUM_CHANNELS_PER_XENGINE */
    uint8_t numFenginePacketsProcessed;/**< Number of F-Engine packets recieved, equal to NUM_ANTENNAS if all packets are received, missing antennas should have their data zeroed*/
    DualPollComplex_in * samples_s;/**< Pointer to the F-Engine samples. The samples_s array should contain NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS*NUM_TIME_SAMPLES DualPollComplex_in samples */
} XGpuPacketIn;

/**
 * @brief: Struct containting the data that is generated by xGPU.
 * 
 * Struct containting the data that is generated by xGPU. The data is arranged as [complexity][channel][station][station](ordered from slowest to fastest changing values).
 * The function getBaselineOffset shows the ordering of the [station][station] section. This ordering is complicated as the baselines are split into quadrants based on whether station1/station2 are odd or even 
 */
typedef struct XGpuPacketOutStruct{
    uint64_t timestamp_u64;/**< Timestamp. */
    uint64_t frequencyBase_u64;/**< Base Frequency of the samples packet. Frequeny in packet run from frequencyBase_u64 to frequencyBase_u64+NUM_CHANNELS_PER_XENGINE */
    BaselineProductsStruct_out * baselines;/**< Pointer to xGpu baselines out. The real and imaginary components are split.*/
} XGpuPacketOut;


typedef struct XGpuInputBufferPacketStruct{
    uint8_t * data_ptr;
    int offset;
} XGpuInputBufferPacket;

typedef struct XGpuOutputBufferPacketStruct{
    uint8_t * data_ptr;
    int offset;
} XGpuOutputBufferPacket;

int getBaselineOffset(int ant0, int ant1);

void displayBaseline(BaselineProducts_out* XGpuPacketOut, int i, int j);

#define DEFAULT_OUTPUT_BUFFERS_THRESHOLD 4

class XGpuBuffers{
    public:
        XGpuBuffers(): lockedLocations_CpuToGpu(0),mutex_array_CpuToGpu(new std::mutex[DEFAULT_ACCUMULATIONS_THRESHOLD]),mutex_array_GpuToCpu(new std::mutex[DEFAULT_OUTPUT_BUFFERS_THRESHOLD]),accessLock_CpuToGpu(),accessLock_GpuToCpu(){
          xgpuInfo(&xgpu_info);
          int xgpu_error = 0;
          index_CpuToGpu=0;
          lockedLocations_CpuToGpu=0;
          context.array_len = xgpu_info.vecLength*DEFAULT_ACCUMULATIONS_THRESHOLD;
          context.matrix_len = xgpu_info.matLength*DEFAULT_OUTPUT_BUFFERS_THRESHOLD;
          context.array_h = (ComplexInput*)malloc(context.array_len*sizeof(ComplexInput));
          context.matrix_h = (Complex*)malloc(context.matrix_len*sizeof(Complex));
          std::cout << ((float)(context.array_len*sizeof(ComplexInput)))/1024/1024/1024 << " GB of gpu storage allocated." << std::endl; 
          xgpu_error = xgpuInit(&context, 0);
          if(xgpu_error) {
              std::cout << "xgpuInit returned error code: " << xgpu_error << std::endl;
          }
        };

        XGPUContext * getXGpuContext_p(){
          return &context;
        }

        //NOTE: I am concerned that the interaction between the wrap around of the index variable and the mutex mechanism will cause a problem - it might make more sense just to have single lock blocking this entire function
        XGpuInputBufferPacket allocateMemory_CpuToGpu(){
          int currentIndex = index_CpuToGpu++ % DEFAULT_ACCUMULATIONS_THRESHOLD;
          //std::cout << currentIndex << " attempted grab" << std::endl;
          mutex_array_CpuToGpu[currentIndex].lock();
          //std::cout << currentIndex << " grabbed" << std::endl;
          XGpuInputBufferPacket structOut;
          structOut.data_ptr = (uint8_t*)(context.array_h + xgpu_info.vecLength*currentIndex);
          //std::cout << (int)structOut.data_ptr<<std::endl;//YOu want to convert this to an iteger and align it
          //std::cout << (int*)structOut.data_ptr<< std::endl;
          //std::cout <<(int)xgpu_info.vecLength << std::endl;
          //std::cout <<(int)currentIndex << std::endl;
          //std::cout << std::endl;
          structOut.offset = currentIndex;
          lockedLocations_CpuToGpu++;
          return structOut;
        }

        void freeMemory_CpuToGpu(int blockToFree){
          mutex_array_CpuToGpu[blockToFree].unlock();
          //std::cout << blockToFree << " freed" << std::endl;
          lockedLocations_CpuToGpu--;
        }


        XGpuOutputBufferPacket allocateMemory_GpuToCpu(){
          int currentIndex = index_GpuToCpu++ % DEFAULT_OUTPUT_BUFFERS_THRESHOLD;
          //std::cout << currentIndex << " attempted grab" << std::endl;
          mutex_array_GpuToCpu[currentIndex].lock();
          //std::cout << currentIndex << " grabbed" << std::endl;
          XGpuOutputBufferPacket structOut;
          structOut.data_ptr = (uint8_t*)(context.matrix_h + xgpu_info.matLength *currentIndex);
          structOut.offset = currentIndex;
          lockedLocations_GpuToCpu++;
          return structOut;
        }

        void freeMemory_GpuToCpu(int blockToFree){
          //std::cout << blockToFree << " ============================" << std::endl;
          mutex_array_GpuToCpu[blockToFree].unlock();
          //std::cout << blockToFree << " freed" << std::endl;
          lockedLocations_GpuToCpu--;
        }

    private:
        XGPUInfo xgpu_info;
        boost::shared_ptr<std::mutex[]> mutex_array_CpuToGpu;
        boost::shared_ptr<std::mutex[]> mutex_array_GpuToCpu;
        std::atomic<int> lockedLocations_CpuToGpu;
        std::atomic<int> index_CpuToGpu;
        std::atomic<int> lockedLocations_GpuToCpu;
        std::atomic<int> index_GpuToCpu;
        XGPUContext context;
        std::mutex accessLock_CpuToGpu;
        std::mutex accessLock_GpuToCpu;
};



class StreamObject{
    public:
        StreamObject(uint64_t timestamp_u64,bool eos,uint64_t frequency): timestamp_u64(timestamp_u64),eos(eos),frequency(frequency){

        }
        StreamObject(bool eos): eos(eos),timestamp_u64(0),frequency(0){

        }
        uint64_t getTimestamp(){
          return timestamp_u64;
        }
        uint64_t getFrequency(){
          return frequency;
        }
        bool isEOS(){
          return eos;
        }
        virtual bool isEmpty(){
          return true;
        }
        friend bool operator<(StreamObject& lhs, StreamObject& rhs)
        {
          return lhs.getTimestamp() < rhs.getTimestamp();
        }
        friend bool operator>(StreamObject& lhs, StreamObject& rhs)
        {
          return lhs.getTimestamp() > rhs.getTimestamp();
        }
    protected:
        uint64_t timestamp_u64;
        const bool eos;
        const uint64_t frequency;
};

class Spead2RxPacket: public StreamObject
{
    public:
        Spead2RxPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,uint64_t fEngineId,uint8_t *payloadPtr_p, boost::shared_ptr<spead2::recv::heap>fheap): StreamObject(timestamp_u64,eos,frequency),fEngineId(fEngineId),fheap(fheap),payloadPtr_p(payloadPtr_p){

        }
        //Spead2RxPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,uint64_t fEngineId): StreamObject(timestamp_u64,eos,frequency),fEngineId(fEngineId){
        //  fheap = nullptr;
        //}
        uint64_t getFEngineId(){
          return fEngineId;
        }
        boost::shared_ptr<spead2::recv::heap> getHeapPtr(){
          return fheap;
        }
        uint8_t * getPayloadPtr_p(){
          return payloadPtr_p;
        }
        //virtual bool isEmpty(){
        //  return(fheap==nullptr);
        //}
        
    protected:
        uint64_t fEngineId;
        uint8_t * payloadPtr_p;
        boost::shared_ptr<spead2::recv::heap> fheap;

};

class BufferPacket: public virtual StreamObject{
    public:
        BufferPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency) : StreamObject(timestamp_u64,eos,frequency),fEnginesPresent_u64(0),numFenginePacketsProcessed(0),heaps_v(){
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
            //std::cout<<(int)this->numFenginePacketsProcessed <<" " << antIndex << std::endl;
            if(!this->isPresent(antIndex)){
              //std::cout << "1" << " " << antIndex <<std::endl;
              heaps_v[antIndex]=fheap;
              data_pointers_v[antIndex] = data_ptr;
              //std::cout << "2" << std::endl;
              numFenginePacketsProcessed++;
              fEnginesPresent_u64 |= 1UL << antIndex;
            }else{
              std::cout << "Received a duplicate packet" << std::endl;
              throw "Received a duplicate packet";
            }
            //std::cout << "4" << std::endl;
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

        //virtual bool isEmpty(){
        //  return heaps_v.size() == 0;
        //}

    protected:
        uint8_t numFenginePacketsProcessed;/**< Number of F-Engine packets recieved, equal to NUM_ANTENNAS if all packets are received, missing antennas should have their data zeroed*/
        uint64_t fEnginesPresent_u64;/**< The bits in this field from 0 to NUM_ANTENNAS will be set to 1 if the corresponding F-Engine packet has been recieved or 0 otherwise.. */
        std::vector<boost::shared_ptr<spead2::recv::heap>> heaps_v;
        std::vector<uint8_t*> data_pointers_v;
};

class Spead2RxPacketWrapper: public virtual StreamObject{
    public:
      Spead2RxPacketWrapper(): StreamObject(false){
      }
      void addPacket(boost::shared_ptr<StreamObject> packetIn){
        packets.push_back(packetIn);
      }
      boost::shared_ptr<StreamObject> removePacket(){
        boost::shared_ptr<StreamObject> outPacket = packets.front();
        packets.pop_front();
        return outPacket;
      }
      int getArmortiserSize(){
        return packets.size();
      }
    private:
      std::deque<boost::shared_ptr<StreamObject>> packets;

};

class ReorderPacket: public virtual StreamObject{
    public:
        ReorderPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,boost::shared_ptr<XGpuBuffers> xGpuBuffer): StreamObject(timestamp_u64,eos,frequency),xGpuBuffer(xGpuBuffer),packetData(xGpuBuffer->allocateMemory_CpuToGpu()){
        
        }
        uint8_t * getDataPointer(){
            return packetData.data_ptr; 
        }
        int getBufferOffset(){
            return packetData.offset;
        }
        ~ReorderPacket(){
          //std::cout << "Cleaning Packet: "<< packetData.offset << std::endl;
          xGpuBuffer->freeMemory_CpuToGpu(packetData.offset);
        }
    private:
        XGpuInputBufferPacket packetData; 
        boost::shared_ptr<XGpuBuffers> xGpuBuffer;

};

class GPUWrapperPacket: public virtual StreamObject{
    public:
        GPUWrapperPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,boost::shared_ptr<XGpuBuffers> xGpuBuffer): StreamObject(timestamp_u64,eos,frequency),xGpuBuffer(xGpuBuffer), packetData(xGpuBuffer->allocateMemory_GpuToCpu()){
        
        }
        uint8_t * getDataPointer(){
            return packetData.data_ptr; 
        }
        int getBufferOffset(){
            return packetData.offset;
        }
        ~GPUWrapperPacket(){
          //std::cout << "Destructor Called " << packetData.offset << std::endl;
          xGpuBuffer->freeMemory_GpuToCpu(packetData.offset);
        }
        
    private:
        XGpuOutputBufferPacket packetData; 
        boost::shared_ptr<XGpuBuffers> xGpuBuffer;

};


typedef tbb::flow::multifunction_node<boost::shared_ptr<StreamObject>, tbb::flow::tuple<boost::shared_ptr<StreamObject> > > multi_node;

struct StreamObjectPointerCompare
{
    bool operator()(const boost::shared_ptr<StreamObject>& lhs, const boost::shared_ptr<StreamObject>& rhs)
    {
        return *lhs > *rhs;
    }
};


//Global Variables;
extern PipelineCounts pipelineCounts;
extern bool debug;
extern int speadTxSuccessCount;


#endif