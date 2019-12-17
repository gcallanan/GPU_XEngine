/**
 * \file x_gpu_pipelined.cpp
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
 #include <queue>
 #include <chrono>
 #include <iomanip>
 #include <boost/program_options.hpp>
 #include <string>
 #include <cstdint>
 #include <boost/asio.hpp>
 #include <boost/shared_ptr.hpp>
 #include <boost/make_shared.hpp>

//Local Includes
#include "global_definitions.h"
#include "Spead2Rx.h"



/** \brief  Main function, launches all threads in the pipeline, exits when all other threads close.
  * \param[in]  argc An integer argument count of the command line arguments
  * \param[in]  argv An argument vector of the command line arguments
  * \return an integer 0 upon exit success
  */

int main(int argc, char** argv){
    //Set up command line arguments
    namespace po = boost::program_options;  
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("rx_port", po::value<int>()->default_value(8888), "Set receiver port")
        ("rx_ip_address", po::value<std::string>()->default_value("none"), "Set the IP address to receive F-Engine data from. Support multicast addresses. If left blank will receive from any unicast IP address.")
        ("tx_port", po::value<std::string>()->default_value("9888"), "Set transmitter port")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")){
        std::cout << desc << "\n";
        return 1;
    }

    std::string rxIpAddress = vm["rx_ip_address"].as<std::string>();
    std::string txPort = vm["tx_port"].as<std::string>();
    int rxPort = vm["rx_port"].as<int>();

    //Multithreading Information
    pipelineCounts.Spead2RxStage=1;
    pipelineCounts.heapsDropped=0;
    pipelineCounts.heapsReceived=0;

    int prevSpead2RxStage=0;

    std::shared_ptr<Spead2Rx> rx; 
    if(strcmp(rxIpAddress.c_str(),"none") == 0){
        rx = std::make_shared<Spead2Rx>(rxPort);
    }else{
        rx = std::make_shared<Spead2Rx>(rxPort,rxIpAddress);
    }
    
    std::cout << "Starting Receiver" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    int heapsDropped_prev = pipelineCounts.heapsDropped;
    int heapsReceived_prev = pipelineCounts.heapsReceived;
    while(true){
        std::this_thread::sleep_for(std::chrono::seconds(10));

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
                   << "Incomplete Heaps: "<< (uint)pipelineCounts.heapsDropped <<" heaps out of "<< (uint)pipelineCounts.heapsReceived << ". Drop Rate Inst/Tot: "<<std::setprecision(4) << float(pipelineCounts.heapsDropped-heapsDropped_prev)/float(pipelineCounts.heapsReceived-heapsReceived_prev)*100 <<  "/" << float(pipelineCounts.heapsDropped)/float(pipelineCounts.heapsReceived)*100 <<" %"<< std::endl
                   << std::endl;

        heapsDropped_prev = pipelineCounts.heapsDropped;
        heapsReceived_prev = pipelineCounts.heapsReceived;
        prevSpead2RxStage = pipelineCounts.Spead2RxStage;
        start=now;

    }
    std::cout<<"Done Receiving Packets" << std::endl;  
    //g.wait_for_all();
    //std::cout<<"All streams finished processing, exiting program."<<std::endl;
    return 0;
}
