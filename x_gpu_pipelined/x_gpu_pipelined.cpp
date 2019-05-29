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
#include "tbb/flow_graph.h"

//Local Includes
#include "Spead2Rx.h"

/// \brief  Main function, launches all threads in the pipeline, exits when all other threads close.
/// \param  argc An integer argument count of the command line arguments
/// \param  argv An argument vector of the command line arguments
/// \return an integer 0 upon exit success
int main(int argc, char** argv){
    tbb::flow::graph g;

    //Construct Graph Nodes
    
    //Construct Edges

    //Start Graph
    Spead2Rx rx;
    int i = 0;
    //spead2::thread_pool worker;
    //std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(262144, 26214400, 64, 64);
    //spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
    //stream.set_memory_allocator(pool);
    //boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), 8888);
    //stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
    
    std::shared_ptr<StreamObject> spead2RxPacket = rx.receive_packet();
    while(spead2RxPacket==nullptr||!spead2RxPacket->isEOS()){
        std::cout << "Receiving Packet: " << i++ << std::endl;
        spead2RxPacket = rx.receive_packet();
    }
    std::cout<<"Done Receiving Packets" << std::endl;
    return 0;
}