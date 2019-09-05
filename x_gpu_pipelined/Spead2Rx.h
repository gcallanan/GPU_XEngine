#include "global_definitions.h"

//Define message passing class

#ifndef _Seapd2Rx_H
#define _Spead2Rx_H

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>



class Spead2Rx{
    class trivial_stream : public spead2::recv::stream
    {
        private:
            virtual void heap_ready(spead2::recv::live_heap &&heap) override;
            std::promise<void> stop_promise;
            multi_node * nextNodeNested;
            boost::shared_ptr<Spead2RxPacketWrapper> outPacketArmortiser;
        public:
            using spead2::recv::stream::stream;
            virtual void stop_received() override;
            void join();
            void addNextNodePointer(multi_node * nextNodeNested);
            void addPacketArmortiser(boost::shared_ptr<Spead2RxPacketWrapper> outPacketArmortiser);

    };
    public:
        Spead2Rx(multi_node * nextNode,int rxPort);
        int getNumCompletePackets();
    private:
        spead2::thread_pool worker;
        trivial_stream stream;
        boost::asio::ip::udp::endpoint endpoint;
        long int n_complete;
        static boost::shared_ptr<StreamObject> process_heap(boost::shared_ptr<spead2::recv::heap> fheap);
        multi_node * nextNode;
        boost::shared_ptr<Spead2RxPacketWrapper> outPacketArmortiser;
};



#endif