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
#include <bitset>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

//Global Defines
#define NUM_ANTENNAS 64
#define NUM_CHANNELS_PER_XENGINE 16 
#define FFT_SIZE (NUM_CHANNELS_PER_XENGINE*4*NUM_ANTENNAS)
#define NUM_POLLS 2
#define NUM_TIME_SAMPLES 256
#define QUADRANT_SIZE ((NUM_ANTENNAS/2+1)*(NUM_ANTENNAS/4))
#define NUM_BASELINES (QUADRANT_SIZE*4)

//Global Structs

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

class StreamObject{
    public:
        StreamObject(uint64_t timestamp_u64,bool eos,uint64_t frequency): timestamp_u64(timestamp_u64),eos(eos),frequency(frequency){

        }
        StreamObject(bool eos): eos(eos){

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
    protected:
        uint64_t timestamp_u64;
        bool eos;
        uint64_t frequency;
};

class Spead2RxPacket: public StreamObject
{
    public:
        Spead2RxPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,uint64_t fEngineId,uint8_t *payloadPtr_p, boost::shared_ptr<spead2::recv::heap>fheap): StreamObject(timestamp_u64,eos,frequency),fEngineId(fEngineId),fheap(fheap){

        }
        uint64_t getFEngineId(){
          return fEngineId;
        }
        boost::shared_ptr<spead2::recv::heap> getHeapPtr(){
          return fheap;
        }
        //virtual bool isEmpty(){
        //  return(fheap==nullptr);
        //}
        
    protected:
        uint64_t fEngineId;
        uint8_t * payloadPtr_p;
        boost::shared_ptr<spead2::recv::heap> fheap;

};

class ReorderPacket: public virtual StreamObject{
    public:
        ReorderPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency) : StreamObject(timestamp_u64,eos,frequency){
            heaps_v.reserve(NUM_ANTENNAS);
        }
        
        void addPacket(int antIndex,boost::shared_ptr<spead2::recv::heap> fheap){
            if(!this->isPresent(antIndex)){
              heaps_v[antIndex] = fheap;
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

        //virtual bool isEmpty(){
        //  return heaps_v.size() == 0;
        //}

    protected:
        uint8_t numFenginePacketsProcessed;/**< Number of F-Engine packets recieved, equal to NUM_ANTENNAS if all packets are received, missing antennas should have their data zeroed*/
        uint64_t fEnginesPresent_u64;/**< The bits in this field from 0 to NUM_ANTENNAS will be set to 1 if the corresponding F-Engine packet has been recieved or 0 otherwise.. */
        std::vector<boost::shared_ptr<spead2::recv::heap>> heaps_v;
};

typedef tbb::flow::multifunction_node<boost::shared_ptr<StreamObject>, tbb::flow::tuple<boost::shared_ptr<StreamObject> > > multi_node;


#endif