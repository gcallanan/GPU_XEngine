#include "global_definitions.h"

//Define message passing class

#ifndef _SpeadRx_H
#define _SpeadRx_H

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <mutex>

#include "PipelinePackets.h"
#include "Buffer.h"
#include <string>

#define NUM_SPEAD2_RX_THREADS 1

class SpeadRx{
    class trivial_stream : public spead2::recv::stream
    {
        private:
            virtual void heap_ready(spead2::recv::live_heap &&heap) override;
            std::promise<void> stop_promise;
            multi_node * nextNodeNested;
            boost::shared_ptr<PacketArmortiser> outPacketArmortiser;
            boost::shared_ptr<Buffer>  buffer;
        public:
            using spead2::recv::stream::stream;
            virtual void stop_received() override;
            void join();
            void addNextNodePointer(multi_node * nextNodeNested);
            void addPacketArmortiser(boost::shared_ptr<PacketArmortiser> outPacketArmortiser);
            void addBuffer(boost::shared_ptr<Buffer> buffer);
            #if NUM_SPEAD2_RX_THREADS > 1
                std::mutex mutex;
            #endif
    };
    public:
        SpeadRx(multi_node * nextNode,int rxPort);
        SpeadRx(multi_node * nextNode,int rxPort, std::string ipAddress);
        int getNumCompletePackets();
    private:
        spead2::thread_pool worker;
        trivial_stream stream;
        boost::asio::ip::udp::endpoint endpoint;
        long int n_complete;
        static boost::shared_ptr<PipelinePacket> process_heap(boost::shared_ptr<spead2::recv::heap> fheap);
        multi_node * nextNode;
        boost::shared_ptr<PacketArmortiser> outPacketArmortiser;
        boost::shared_ptr<Buffer>  buffer;
};



#endif