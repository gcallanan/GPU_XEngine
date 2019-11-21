/**
 * \file xGpu_TBB.cpp
 *
 * \brief This is the main file for the MeerKAT GPU X-Engine program. It launches all the threads in the pipeline.
 *
 * \author Gareth Callanan
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
#include "SpeadRx.h"
#include "Buffer.h"
#include "Transpose.h"
#include "GPUWrapper.h"
#include "SpeadTx.h"
#include "XGpuBufferManager.h"
#include "PipelinePackets.h"


/** \brief  Main function, launches all threads in the pipeline, exits when all other threads close.
  * \param[in]  argc An integer argument count of the command line arguments
  * \param[in]  argv An argument vector of the command line arguments
  * \return an integer 0 upon exit success
  */

typedef std::vector<boost::shared_ptr<multi_node> > NodePointersVector;

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
    speadTxSuccessCount = 0;
    pipelineCounts.SpeadRxStage=1;
    pipelineCounts.BufferStage=1;
    for (size_t i = 0; i < NUM_TRANSPOSE_STAGES; i++){
        pipelineCounts.TransposeStage[i]=1;
    }
    pipelineCounts.GPUWRapperStage=1;
    pipelineCounts.SpeadTxStage=1;
    pipelineCounts.heapsDropped=0;
    pipelineCounts.heapsReceived=0;
    pipelineCounts.packetsTooLate=0;

    int prevSpeadRxStage=0;
    int prevBufferStage=0;
    int prevTransposeStage[NUM_TRANSPOSE_STAGES];
    for (size_t i = 0; i < NUM_TRANSPOSE_STAGES; i++){
        prevTransposeStage[i]=0;
    }
    int prevGPUWrapperStage=0;
    int prevSpeadTxStage=0;
    int prevPacketsTooLate=0;

    boost::shared_ptr<XGpuBufferManager> xGpuBuffer = boost::make_shared<XGpuBufferManager>();
    //Construct Graph Nodes

    //multi_node bufferNode(g,1,Buffer());
    std::vector<boost::shared_ptr<multi_node> > transposeStagesList;
    for (size_t i = 0; i < NUM_TRANSPOSE_STAGES; i++)
    {
       transposeStagesList.push_back(boost::make_shared<multi_node>(g,1,Transpose(xGpuBuffer,i)));
    }
    multi_node gpuNode(g,1,GPUWrapper(xGpuBuffer));
    multi_node txNode(g,1,SpeadTx(txPort));
    SpeadRx rx(&(*transposeStagesList[0]),rxPort);
    
    //Construct Edges
    for (size_t i = 1; i < NUM_TRANSPOSE_STAGES; i++)
    {
        tbb::flow::make_edge(tbb::flow::output_port<0>(*transposeStagesList[i-1]), *transposeStagesList[i]);
    }
    //tbb::flow::make_edge(tbb::flow::output_port<0>(*transposeStagesList[NUM_TRANSPOSE_STAGES-1]), gpuNode);
    tbb::flow::make_edge(tbb::flow::output_port<0>(gpuNode),txNode);

    //Start Graph
    std::cout << "Transpose Block Size per stage: " << (NUM_ANTENNAS/NUM_TRANSPOSE_STAGES) << std::endl;
    std::cout << "Starting Graph" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    int heapsDropped_prev = pipelineCounts.heapsDropped;
    int heapsReceived_prev = pipelineCounts.heapsReceived;
    while(true){
        std::this_thread::sleep_for (std::chrono::seconds(10));

        //Reporting Code
        uint numPacketsReceivedComplete = (uint)pipelineCounts.SpeadRxStage - prevSpeadRxStage;
        uint numPacketsReceivedIncomplete = (uint)pipelineCounts.heapsReceived - heapsReceived_prev;
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = now-start;
        double bits_received_complete = ((double)numPacketsReceivedComplete*NUM_TIME_SAMPLES*NUM_CHANNELS_PER_XENGINE*NUM_POLLS*2*8);
        double bits_received_incomplete = ((double)numPacketsReceivedIncomplete*NUM_TIME_SAMPLES*NUM_CHANNELS_PER_XENGINE*NUM_POLLS*2*8);
	    double num_packets_received_transpose = (double)((uint)pipelineCounts.TransposeStage[0] - prevTransposeStage[0])*64*NUM_TIME_SAMPLES*NUM_CHANNELS_PER_XENGINE*NUM_POLLS*2*8;
        std::cout <<"Complete Heaps Rate  : "<<std::fixed<<std::setprecision(2)<< bits_received_complete/1000/1000/1000 << " Gbits received in "<<diff.count()<<" seconds. Data Rate: " <<bits_received_complete/1000/1000/1000/diff.count() << " Gbps" << std::endl;
        std::cout <<"Incomplete Heaps Rate: "<<std::fixed<<std::setprecision(2)<< bits_received_incomplete/1000/1000/1000 << " Gbits received in "<<diff.count()<<" seconds. Data Rate: " <<bits_received_incomplete/1000/1000/1000/diff.count() << " Gbps" << std::endl;
        std::cout <<"Processed Heaps Rate : "<<std::fixed<<std::setprecision(2)<< num_packets_received_transpose/1000/1000/1000 << " Gbits received in "<<diff.count()<<" seconds. Data Rate: " <<num_packets_received_transpose/1000/1000/1000/diff.count() << " Gbps" << std::endl;
        std::cout   << "HeapsReceived                  : " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.heapsReceived << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << (uint)pipelineCounts.heapsReceived - heapsReceived_prev <<std::endl
                    << "SpeadRx       Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.SpeadRxStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << (uint)pipelineCounts.SpeadRxStage - prevSpeadRxStage <<std::endl
                    << "Buffer        Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.BufferStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.BufferStage - prevBufferStage) <<std::endl
                    << "Transpose "<<0<<"   Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.TransposeStage[0] << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.TransposeStage[0] - prevTransposeStage[0])*64 <<std::endl;
        for (size_t i = 1; i < NUM_TRANSPOSE_STAGES; i++){
                    std::cout << "Transpose "<<i<<"   Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.TransposeStage[i] << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.TransposeStage[i] - prevTransposeStage[i])*64 <<std::endl;
        }
        std::cout   << "GPUWrapper    Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.GPUWRapperStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.GPUWRapperStage - prevGPUWrapperStage)*64 <<std::endl
                    << "SpeadTx       Packets Processed: " << std::setfill(' ') << std::setw(10) << (uint)pipelineCounts.SpeadTxStage << " Normalised Diff:"<< std::setfill(' ') << std::setw(7) << ((uint)pipelineCounts.SpeadTxStage - prevSpeadTxStage)*64*400 <<std::endl
                    << "Incomplete Heaps: "<< (uint)pipelineCounts.heapsDropped <<" heaps out of "<< (uint)pipelineCounts.heapsReceived << ". Drop Rate Inst/Tot: "<<std::setprecision(4) << float(pipelineCounts.heapsDropped-heapsDropped_prev)/float(pipelineCounts.heapsReceived-heapsReceived_prev)*100 <<  "/" << float(pipelineCounts.heapsDropped)/float(pipelineCounts.heapsReceived)*100 <<" %"<< std::endl
                    << "Heaps Too Late: " << (uint)pipelineCounts.packetsTooLate << ". Diff: " << ((uint)pipelineCounts.packetsTooLate - prevPacketsTooLate) << ". Instantaneus Percentage Late:" <<((float)((uint)pipelineCounts.packetsTooLate - prevPacketsTooLate))/((float)(pipelineCounts.heapsReceived-heapsReceived_prev))*100<<"%"<<std::endl
                    << std::endl;

        heapsDropped_prev = pipelineCounts.heapsDropped;
        heapsReceived_prev = pipelineCounts.heapsReceived;

        if((pipelineCounts.BufferStage-prevBufferStage) == 0){
            if(((uint)pipelineCounts.SpeadRxStage - prevSpeadRxStage)==0){
                std::cout << "No packets received from input stream." << std::endl;
            }else{
                std::cout << "ERROR: Pipeline has stalled. Exiting Program" << std::endl; 
                break;//Not strictly necessary, streams should reset when the packet flow resumes, this is just in for debugging as sometimes a pipeline stage crashes and this causes a memory leak.
            }
        }

        prevSpeadRxStage = pipelineCounts.SpeadRxStage; 
        prevBufferStage = pipelineCounts.BufferStage;
        for (size_t i = 0; i < NUM_TRANSPOSE_STAGES; i++){
            prevTransposeStage[i] = pipelineCounts.TransposeStage[i];
        }
        prevGPUWrapperStage = pipelineCounts.GPUWRapperStage;
        prevSpeadTxStage = pipelineCounts.SpeadTxStage;
        prevPacketsTooLate = pipelineCounts.packetsTooLate;

        start=now;
    }
    std::cout<<"Done Receiving Packets" << std::endl;  
    g.wait_for_all();
    //std::cout<<"All streams finished processing, exiting program."<<std::endl;
    return 0;
}
