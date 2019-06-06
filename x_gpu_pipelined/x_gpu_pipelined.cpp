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

//Local Includes
#include "Spead2Rx.h"
#include "Buffer.h"
#include "Reorder.h"
#include "GPUWrapper.h"
#include "global_definitions.h"

#define REPORTING_PACKETS_COUNT 100000

/// \brief  Main function, launches all threads in the pipeline, exits when all other threads close.
/// \param  argc An integer argument count of the command line arguments
/// \param  argv An argument vector of the command line arguments
/// \return an integer 0 upon exit success
int main(int argc, char** argv){
    tbb::flow::graph g;

    //Multithreading Information
    pipelineCounts.Spead2Stage=0;
    pipelineCounts.BufferStage=0;
    pipelineCounts.ReorderStage=0;
    pipelineCounts.GPUWRapperStage=0;

    int prevSpead2Stage=0;//.load(pipelineCounts.Spead2Stage);
    int prevBufferStage=0;//.load(pipelineCounts.BufferStage);
    int prevReorderStage=0;//.load(pipelineCounts.ReorderStage);
    int prevGPUWrapperStage=0;

    boost::shared_ptr<XGpuBuffers> xGpuBuffer = boost::make_shared<XGpuBuffers>();
    //Construct Graph Nodes
    multi_node bufferNode(g,1,Buffer());
    multi_node reorderNode(g,1,Reorder(xGpuBuffer));
    multi_node gpuNode(g,1,GPUWrapper(xGpuBuffer));
    
    //Construct Edges
    tbb::flow::make_edge(tbb::flow::output_port<0>(bufferNode), reorderNode);
    tbb::flow::make_edge(tbb::flow::output_port<0>(reorderNode), gpuNode);

    //Start Graph
    std::cout << "Starting Graph" << std::endl;
    Spead2Rx rx;
    int i = 0;    
    boost::shared_ptr<StreamObject> spead2RxPacket = rx.receive_packet();
    auto start = std::chrono::high_resolution_clock::now();
    while(spead2RxPacket==nullptr || !spead2RxPacket->isEOS()){

        //Reporting Code
        if(rx.getNumCompletePackets() % REPORTING_PACKETS_COUNT == 0){
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = now-start;
            double bits_received = ((double)REPORTING_PACKETS_COUNT*NUM_TIME_SAMPLES*NUM_CHANNELS_PER_XENGINE*NUM_POLLS*2*8);
            std::cout <<std::fixed<<std::setprecision(2)<< bits_received/1000/1000/1000 << " Gbits received in "<<diff.count()<<" seconds. Data Rate: " <<bits_received/1000/1000/1000/diff.count() << " Gbps" << std::endl;
            std::cout << "Spead2Rx    Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.Spead2Stage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << (uint)pipelineCounts.Spead2Stage - prevSpead2Stage <<std::endl
                      << "Buffer      Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.BufferStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << (uint)pipelineCounts.BufferStage - prevBufferStage <<std::endl
                      << "Reorder     Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.ReorderStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.ReorderStage - prevReorderStage)*64 <<std::endl
                      << "GPUWrapper  Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.GPUWRapperStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.GPUWRapperStage - prevGPUWrapperStage)*64 <<std::endl
                      << std::endl;

            if((pipelineCounts.BufferStage-prevBufferStage) == 0){
                debug = true;
                std::cout << "Debug Set to true: "<<debug << std::endl
                        << std::endl;
                break;
            }

            prevSpead2Stage = pipelineCounts.Spead2Stage;
            prevBufferStage = pipelineCounts.BufferStage;
            prevReorderStage = pipelineCounts.ReorderStage;
            prevGPUWrapperStage = pipelineCounts.GPUWRapperStage;

            start=now;
        }

        //Rx Code
        if(spead2RxPacket!=nullptr){
            if(!bufferNode.try_put(spead2RxPacket)){
                std::cout << "Packet Failed to be passed to buffer class" << std::endl;
            }
        }

        spead2RxPacket = rx.receive_packet();
    }
    std::cout<<"Done Receiving Packets" << std::endl;  
    g.wait_for_all();
    std::cout<<"All streams finished processing, exiting program."<<std::endl;
    return 0;
}