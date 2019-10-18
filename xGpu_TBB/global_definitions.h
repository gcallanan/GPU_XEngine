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
#define DEFAULT_ACCUMULATIONS_THRESHOLD ((int)(1632*3))

/** Before packets are handed over to the next stage of the pipeline they are grouped into a larger packet to reduce thread overhead. ARMORTISER_SIZE specifies the number of packets to group*/
#define ARMORTISER_SIZE 300

/** Same function as ARMORTISER_SIZE but specifically implemented for the transmission from the Transpose to the GPUWrapper class. This class needs to have a smaller ARMORTISER_SIZE as the packets q ueued here are holding GPU memory which we want to use efficiently */
#define ARMORTISER_TO_GPU_SIZE 20

//Transpose 
/** Number of stages for the reoder pipeline module. Each stage is processed by a different thread */
#define NUM_TRANSPOSE_STAGES 2
/** Block size to perform the transpose, must be a power of two.*/


//Global Structs

/**
 * Struct counters for different stages in the pipeline. This is used to monitor flow rates through the pipeline
 */
typedef struct PipelineCountsStruct{
   std::atomic<int> SpeadRxStage; /**< Number of packets processed by Spead2Rx pipeline.*/
   std::atomic<int> BufferStage; /**< Number of packets processed by Buffer pipeline stage. */
   std::atomic<int> TransposeStage[NUM_TRANSPOSE_STAGES]; /**< Number of packets processed by by the Transpose pipeline class. */
   std::atomic<int> GPUWRapperStage; /**< Number of packets processed by the GPUWrapper pipeline class. */
   std::atomic<int> SpeadTxStage; /**< Number of packets processed by SpeadTx class. */
   std::atomic<int> heapsDropped; /**< Number of heaps dropped due to not receiving all ethernet packets. */
   std::atomic<int> heapsReceived; /**< Total number of heaps received.(Complete and dropped) */
   std::atomic<int> packetsTooLate; /**< Packets received by Buffer class that were dropped as the timestamp is too late to arrive */
} PipelineCounts;

/**
 * Struct storing dual pol complex samples as received by the F-Engines
 */
typedef struct DualPollComplexStruct_in {
  int8_t realPol0;/**< Pol 0 Real Component. */
  int8_t imagPol0;/**< Pol 0 Imaginary Component. */
  int8_t realPol1;/**< Pol 1 Real Component. */
  int8_t imagPol1;/**< Pol 1 Imaginary Component. */
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

//Global Variables;
extern PipelineCounts pipelineCounts;


#endif
