#ifndef _SPEADTX_H
#define _SPEADTX_H

#include <boost/asio.hpp>
#include <spead2/common_endian.h>
#include <spead2/common_thread_pool.h>
#include <spead2/common_defines.h>
#include <spead2/common_flavour.h>
#include <spead2/send_heap.h>
#include <spead2/send_udp.h>
#include <spead2/send_stream.h>
#include <iostream>

#include "tbb/flow_graph.h"
#include "global_definitions.h"

class SpeadTx{
    /*class ArrayWrapper{
        public:
            ArrayWrapper(){
                std::cout << "Allocated" << std::endl;
                arrayPtr = new int32_t[NUM_BASELINES*NUM_CHANNELS_PER_XENGINE*4*2];//4 For the number of products per baseline and 2 for real/complexity
            }
            ~ArrayWrapper(){
                std::cout << "Deallocated" << std::endl;
                delete [] arrayPtr; 
            }
            int32_t * getArrayPtr(){
                return arrayPtr;
            }
            int32_t ** getArrayPtrPtr(){
                return &arrayPtr;
            }
            int32_t getSize_bytes(){
                return NUM_BASELINES*NUM_CHANNELS_PER_XENGINE*4*2;
            }
        private:
            int32_t * arrayPtr;
    };*/
    public:
        SpeadTx(std::string txPort);
        void operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op);     
    private:
        boost::shared_ptr<spead2::thread_pool> tp;
        boost::shared_ptr<boost::asio::ip::udp::resolver> resolver;
        boost::shared_ptr<boost::asio::ip::udp::resolver::query> query;
        boost::shared_ptr<boost::asio::ip::basic_resolver_iterator<boost::asio::ip::udp>> it;
        boost::shared_ptr<spead2::send::udp_stream> stream;
        boost::shared_ptr<spead2::flavour> f;
};

#endif