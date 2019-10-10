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
#include "PipelinePackets.h"
#include "PipelineStages.h"

class SpeadTx : public PipelineStage{
    public:
        SpeadTx(std::string txPort);
        OutputPacketQueuePtr processPacket(boost::shared_ptr<PipelinePacket> inPacket);    
    private:
        boost::shared_ptr<spead2::thread_pool> tp;
        boost::shared_ptr<boost::asio::ip::udp::resolver> resolver;
        boost::shared_ptr<boost::asio::ip::udp::resolver::query> query;
        boost::shared_ptr<boost::asio::ip::basic_resolver_iterator<boost::asio::ip::udp>> it;
        boost::shared_ptr<spead2::send::udp_stream> stream;
        boost::shared_ptr<spead2::flavour> f;
};

#endif