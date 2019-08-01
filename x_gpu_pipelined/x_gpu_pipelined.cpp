/**
 * @file x_gpu_pipelined.cpp
 *
 * @brief This is the main file for the MeerKAT GPU X-ENgine program. It launches all the threads in the pipeline
 *
 * @author Gareth Callanan
 *
 */

//Library Includes
#include <iostream>
#include <stdio.h>
#include <deque>
#include "tbb/flow_graph.h"
#include <queue>
#include <chrono>
#include <iomanip>
#include <boost/program_options.hpp>
#include <string>

//Local Includes
#include "global_definitions.h"

#include "Spead2Rx.h"
#include "Buffer.h"
#include "Reorder.h"
#include "GPUWrapper.h"
#include "SpeadTx.h"
#include <emmintrin.h>


#define REPORTING_PACKETS_COUNT 100000

/// \brief  Main function, launches all threads in the pipeline, exits when all other threads close.
/// \param  argc An integer argument count of the command line arguments
/// \param  argv An argument vector of the command line arguments
/// \return an integer 0 upon exit success
int main(int argc, char** argv){
    //Set up command line arguments
    namespace po = boost::program_options;  
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("rx_port", po::value<int>()->default_value(8888), "Set receiver port")
        ("tx_port", po::value<std::string>()->default_value("9888"), "Set transmitter port")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")){
        std::cout << desc << "\n";
        return 1;
    }

    #ifndef __AVX__
    std::cout<<"AVX not supported"<<std::endl;
    #endif

    std::string txPort = vm["tx_port"].as<std::string>();
    int rxPort = vm["rx_port"].as<int>();
    //Create flow graph
    tbb::flow::graph g;

    //Multithreading Information
    pipelineCounts.Spead2RxStage=1;
    pipelineCounts.BufferStage=1;
    pipelineCounts.ReorderStage=1;
    pipelineCounts.GPUWRapperStage=1;
    pipelineCounts.Spead2TxStage=1;
    pipelineCounts.heapsDropped=0;
    pipelineCounts.heapsReceived=0;
    pipelineCounts.packetsTooLate=0;

    int prevSpead2RxStage=0;//.load(pipelineCounts.Spead2Stage);
    int prevBufferStage=0;//.load(pipelineCounts.BufferStage);
    int prevReorderStage=0;//.load(pipelineCounts.ReorderStage);
    int prevGPUWrapperStage=0;
    int prevSpead2TxStage=0;
    int prevPacketsTooLate=0;

    boost::shared_ptr<XGpuBuffers> xGpuBuffer = boost::make_shared<XGpuBuffers>();
    //Construct Graph Nodes
    multi_node bufferNode(g,1,Buffer());
    multi_node reorderNode(g,1,Reorder(xGpuBuffer));//tbb::flow::unlimited
    multi_node gpuNode(g,1,GPUWrapper(xGpuBuffer));
    multi_node txNode(g,1,SpeadTx(txPort));
    Spead2Rx rx(&bufferNode,rxPort);
    
    //Construct Edges
    tbb::flow::make_edge(tbb::flow::output_port<0>(bufferNode), reorderNode);
    tbb::flow::make_edge(tbb::flow::output_port<0>(reorderNode), gpuNode);
    tbb::flow::make_edge(tbb::flow::output_port<0>(gpuNode),txNode);


    //Temporary, remove before production
    int a = 12;
    int b = 64;
    std::cout << a << " " << b << std::endl;
    _mm_stream_si32(&a,b);
    
    std::cout << a << " " << b << std::endl;
    int32_t a_arr[4] = {1,2,3,4};

    std::cout << a_arr[0] << " " << a_arr[1] << " " << a_arr[2] << " " << a_arr[3] << std::endl;
    __m128i row = _mm_setr_epi32(10,20,30,40);
    _mm_store_si128((__m128i*)a_arr,row);
    std::cout << a_arr[0] << " " << a_arr[1] << " " << a_arr[2] << " " << a_arr[3] << std::endl;

    //Start Graph
    std::cout << "Starting Graph" << std::endl;
    
    int i = 0;    
    //boost::shared_ptr<StreamObject> spead2RxPacket = rx.receive_packet();
    auto start = std::chrono::high_resolution_clock::now();
    //while(spead2RxPacket==nullptr || !spead2RxPacket->isEOS()){
    int heapsDropped_prev = pipelineCounts.heapsDropped;
    int heapsReceived_prev = pipelineCounts.heapsReceived;
    while(true){
        std::this_thread::sleep_for (std::chrono::seconds(10));

        //Reporting Code
        uint numPacketsReceivedComplete = (uint)pipelineCounts.Spead2RxStage - prevSpead2RxStage;
        uint numPacketsReceivedIncomplete = (uint)pipelineCounts.heapsReceived - heapsReceived_prev;
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = now-start;
        double bits_received_complete = ((double)numPacketsReceivedComplete*NUM_TIME_SAMPLES*NUM_CHANNELS_PER_XENGINE*NUM_POLLS*2*8);
        double bits_received_incomplete = ((double)numPacketsReceivedIncomplete*NUM_TIME_SAMPLES*NUM_CHANNELS_PER_XENGINE*NUM_POLLS*2*8);
        std::cout <<"Complete Heaps Rate  : "<<std::fixed<<std::setprecision(2)<< bits_received_complete/1000/1000/1000 << " Gbits received in "<<diff.count()<<" seconds. Data Rate: " <<bits_received_complete/1000/1000/1000/diff.count() << " Gbps" << std::endl;
        std::cout <<"Incomplete Heaps Rate: "<<std::fixed<<std::setprecision(2)<< bits_received_incomplete/1000/1000/1000 << " Gbits received in "<<diff.count()<<" seconds. Data Rate: " <<bits_received_incomplete/1000/1000/1000/diff.count() << " Gbps" << std::endl;
        std::cout   << "HeapsReceived                : " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.heapsReceived << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << (uint)pipelineCounts.heapsReceived - heapsReceived_prev <<std::endl
                    << "Spead2Rx    Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.Spead2RxStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << (uint)pipelineCounts.Spead2RxStage - prevSpead2RxStage <<std::endl
                    << "Buffer      Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.BufferStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.BufferStage - prevBufferStage)*64*ARMORTISER_SIZE <<std::endl
                    << "Reorder     Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.ReorderStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.ReorderStage - prevReorderStage)*64*ARMORTISER_SIZE <<std::endl
                    << "GPUWrapper  Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.GPUWRapperStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.GPUWRapperStage - prevGPUWrapperStage)*64*ARMORTISER_SIZE <<std::endl
                    << "Spead2Tx    Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.Spead2TxStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.Spead2TxStage - prevSpead2TxStage)*64*1600 <<std::endl
                    << "Incomplete Heaps: "<< (uint)pipelineCounts.heapsDropped <<" heaps out of "<< (uint)pipelineCounts.heapsReceived << ". Drop Rate Inst/Tot: "<<std::setprecision(4) << float(pipelineCounts.heapsDropped-heapsDropped_prev)/float(pipelineCounts.heapsReceived-heapsReceived_prev)*100 <<  "/" << float(pipelineCounts.heapsDropped)/float(pipelineCounts.heapsReceived)*100 <<" %"<< std::endl
                    << "Heaps Too Late: " << (uint)pipelineCounts.packetsTooLate << ". Diff: " << ((uint)pipelineCounts.packetsTooLate - prevPacketsTooLate) << ". Instantaneus Percentage Late:" <<((float)((uint)pipelineCounts.packetsTooLate - prevPacketsTooLate))/((float)(pipelineCounts.heapsReceived-heapsReceived_prev))*100<<"%"<<std::endl
                    << std::endl;

        heapsDropped_prev = pipelineCounts.heapsDropped;
        heapsReceived_prev = pipelineCounts.heapsReceived;

        if((pipelineCounts.BufferStage-prevBufferStage) == 0){
            debug = true;
            std::cout << "Debug Set to true: "<<debug << std::endl
                    << std::endl;
            break;
        }

        prevSpead2RxStage = pipelineCounts.Spead2RxStage;
        prevBufferStage = pipelineCounts.BufferStage;
        prevReorderStage = pipelineCounts.ReorderStage;
        prevGPUWrapperStage = pipelineCounts.GPUWRapperStage;
        prevSpead2TxStage = pipelineCounts.Spead2TxStage;
        prevPacketsTooLate = pipelineCounts.packetsTooLate;

        start=now;

        //Rx Code
        //if(spead2RxPacket!=nullptr){
        //    if(!bufferNode.try_put(spead2RxPacket)){
        //        std::cout << "Packet Failed to be passed to buffer class" << std::endl;
        //   }
        //}

        //spead2RxPacket = rx.receive_packet();
    }
    std::cout<<"Done Receiving Packets" << std::endl;  
    //g.wait_for_all();
    std::cout<<"All streams finished processing, exiting program."<<std::endl;
    return 0;
}