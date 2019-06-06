#include "global_definitions.h"

//Define message passing class

#ifndef _Seapd2Rx_H
#define _Spead2Rx_H

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

class Spead2Rx{
    public:
        Spead2Rx();
        boost::shared_ptr<StreamObject> receive_packet();
        int getNumCompletePackets();
    private:
        spead2::thread_pool worker;
        std::shared_ptr<spead2::memory_pool> pool;
        spead2::recv::ring_stream<> stream;
        boost::asio::ip::udp::endpoint endpoint;
        long int n_complete;
        static boost::shared_ptr<StreamObject> process_heap(boost::shared_ptr<spead2::recv::heap> fheap);
};

#endif