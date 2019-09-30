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
//#include <boost/shared_ptr.hpp>
//#include <boost/make_shared.hpp>
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

//Global Structs

/**
 * Struct counters for different stages in the pipeline. This is used to monitor flow rates through the pipeline
 */
typedef struct PipelineCountsStruct{
   std::atomic<int> Spead2RxStage; /**< Number of packets processed by Spead2Rx pipeline.*/
   std::atomic<int> BufferStage; /**< Number of packets processed by Buffer pipeline stage. */
   //std::atomic<int> ReorderStage[NUM_REORDER_STAGES]; /**< Number of packets processed by by the Reorder pipeline class. */
   std::atomic<int> GPUWRapperStage; /**< Number of packets processed by the GPUWrapper pipeline class. */
   std::atomic<int> Spead2TxStage; /**< Number of packets processed by SpeadTx class. */
   std::atomic<int> heapsDropped; /**< Number of heaps dropped due to not receiving all ethernet packets. */
   std::atomic<int> heapsReceived; /**< Total number of heaps received.(Complete and dropped) */
   std::atomic<int> packetsTooLate; /**< Packets received by Buffer class that were dropped as the timestamp is too late to arrive */
} PipelineCounts;


extern PipelineCounts pipelineCounts;


#endif
