#include <boost/asio.hpp>
#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#include <spead2/recv_udp_pcap.h>
#include <spead2/recv_heap.h>
#include <spead2/recv_live_heap.h>
#include <spead2/recv_ring_stream.h>
#include <bitset>

#include "global_definitions.h"

//Define message passing class

class Spead2Rx{
    public:
        Spead2Rx();
        std::shared_ptr<StreamObject> receive_packet();
    private:
        spead2::thread_pool worker;
        std::shared_ptr<spead2::memory_pool> pool;
        spead2::recv::ring_stream<> stream;
        boost::asio::ip::udp::endpoint endpoint;
        long int n_complete;
        static std::shared_ptr<StreamObject> process_heap(spead2::recv::heap &fheap);
};