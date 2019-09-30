#include "global_definitions.h"

//Define message passing class

#ifndef _Spead2Rx_H
#define _Spead2Rx_H

//#include <boost/shared_ptr.hpp>
//#include <boost/make_shared.hpp>
#include <mutex>

#define NUM_SPEAD2_RX_THREADS 1

class Spead2Rx{
    class trivial_stream : public spead2::recv::stream
    {
        private:
            virtual void heap_ready(spead2::recv::live_heap &&heap) override;
            std::promise<void> stop_promise;
        public:
            using spead2::recv::stream::stream;
            virtual void stop_received() override;
            void join();
            #if NUM_SPEAD2_RX_THREADS > 1
                std::mutex mutex;
            #endif

    };
    public:
        Spead2Rx(int rxPort);
        int getNumCompletePackets();
    private:
        spead2::thread_pool worker;
        trivial_stream stream;
        boost::asio::ip::udp::endpoint endpoint;
        long int n_complete;
};



#endif