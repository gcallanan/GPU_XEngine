#include "global_definitions.h"

//Define message passing class

#ifndef _SpeadRxHashpipe_H
#define _SpeadRxHashpipe_H

#include <boost/asio.hpp>
#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#include <spead2/recv_udp_pcap.h>
#include <spead2/recv_heap.h>
#include <spead2/recv_live_heap.h>
#include <spead2/recv_ring_stream.h>
#include <spead2/recv_stream.h>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <mutex>
#include "PipelinePackets.h"
#include <hashpipe.h>

#define NUM_SPEAD2_RX_THREADS 1

class SpeadRxHashpipe{
    class trivial_stream : public spead2::recv::stream
    {
        private:
            virtual void heap_ready(spead2::recv::live_heap &&heap) override;
            std::promise<void> stop_promise;
            multi_node * nextNodeNested;
            boost::shared_ptr<PacketArmortiser> outPacketArmortiser;
        public:
            using spead2::recv::stream::stream;
            virtual void stop_received() override;
            void join();
            void addNextNodePointer(multi_node * nextNodeNested);
            void addPacketArmortiser(boost::shared_ptr<PacketArmortiser> outPacketArmortiser);
            #if NUM_SPEAD2_RX_THREADS > 1
                std::mutex mutex;
            #endif
    };
    public:
        SpeadRxHashpipe(hashpipe_thread_args_t * hashpipe_args_in,int rxPort);
        int getNumCompletePackets();
    private:
        spead2::thread_pool worker;
        trivial_stream stream;
        boost::asio::ip::udp::endpoint endpoint;
        long int n_complete;
        static boost::shared_ptr<PipelinePacket> process_heap(boost::shared_ptr<spead2::recv::heap> fheap);
        boost::shared_ptr<PacketArmortiser> outPacketArmortiser;
        static hashpipe_thread_args_t * hashpipe_args;
};



#endif