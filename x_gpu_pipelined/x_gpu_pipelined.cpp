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

//Local Includes
#include "Spead2Rx.h"
#include "Reorder.h"
#include "global_definitions.h"

/// \brief  Main function, launches all threads in the pipeline, exits when all other threads close.
/// \param  argc An integer argument count of the command line arguments
/// \param  argv An argument vector of the command line arguments
/// \return an integer 0 upon exit success
int main(int argc, char** argv){
    tbb::flow::graph g;

    //Construct Graph Nodes
    multi_node reorderNode(g,tbb::flow::unlimited,Reorder());
    //Construct Edges
    //Start Graph
    std::cout << "Starting Graph" << std::endl;
    Spead2Rx rx;
    int i = 0;    
    boost::shared_ptr<StreamObject> spead2RxPacket = rx.receive_packet();
    while(spead2RxPacket==nullptr || !spead2RxPacket->isEOS()){
        //std::cout << "Receiving Packet: " << i++ <<std::endl;
        spead2RxPacket = rx.receive_packet();
        if(spead2RxPacket!=nullptr){
            //std::cout << "Timestamp: " << spead2RxPacket->getTimestamp() << std::endl;
            reorderNode.try_put(spead2RxPacket);
        }else{
            //Descriptors received
        }
    }
    std::cout<<"Done Receiving Packets" << std::endl;  
    g.wait_for_all();
    std::cout<<"All streams finished processing, exiting program."<<std::endl;
    return 0;
}