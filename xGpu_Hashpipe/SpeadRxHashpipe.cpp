#include "SpeadRxHashpipe.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "xGpu_Hashpipe_databuf.h"



SpeadRxHashpipe::SpeadRxHashpipe(hashpipe_thread_args_t * hashpipe_args_in,int rxPort): worker(NUM_SPEAD2_RX_THREADS),stream(worker),n_complete(0),endpoint(boost::asio::ip::address_v4::any(), rxPort){
    hashpipe_args = hashpipe_args_in;
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
    outPacketArmortiser = boost::make_shared<PacketArmortiser>();
    stream.addPacketArmortiser(outPacketArmortiser);
}

boost::shared_ptr<PipelinePacket> SpeadRxHashpipe::process_heap(boost::shared_ptr<spead2::recv::heap> fheap){
    const auto &items = fheap->get_items();
    uint64_t fengId;
    uint64_t timestamp;
    uint64_t frequency;
    uint8_t * payloadPtr_p;
    bool fengPacket = false;
    
    for (const auto &item : items)
    {
        if(item.id == 0x1600){
          timestamp = item.immediate_value;
        }

        if(item.id == 0x4101){
          fengId = item.immediate_value;
        }

        if(item.id == 0x4103){
          frequency = item.immediate_value;
        }

        if(item.id == 0x4300){
          payloadPtr_p = item.ptr;
          fengPacket = true;
        }
    }

    if(fengPacket){
        return boost::make_shared<SpeadRxPacket>(timestamp,false,frequency,fengId,payloadPtr_p,fheap);
    }else{
        std::cout << "Unidentified Packet Received. This may be a broken F-Engine packet" << std::endl;
        return boost::shared_ptr<SpeadRxPacket>(nullptr);
    }
}

int SpeadRxHashpipe::getNumCompletePackets(){
  return n_complete;
}

void SpeadRxHashpipe::trivial_stream::heap_ready(spead2::recv::live_heap &&heap)
{
    pipelineCounts.heapsReceived++;
    if (heap.is_complete())
    {   
        boost::shared_ptr<PipelinePacket> SpeadRxPacket = process_heap(boost::make_shared<spead2::recv::heap>(boost::move(heap)));
        OutputPacketQueuePtr outputPacketQueuePtr = boost::make_shared<OutputPacketQueue>();
        outputPacketQueuePtr->push_back(SpeadRxPacket);
        writeOut(hashpipe_args,outputPacketQueuePtr);
    }else{
      pipelineCounts.heapsDropped++;
    }
}


void SpeadRxHashpipe::trivial_stream::stop_received()
{
    spead2::recv::stream::stop_received();
    stop_promise.set_value();
}

void SpeadRxHashpipe::trivial_stream::join()
{
    std::future<void> future = stop_promise.get_future();
    future.get();
}

void SpeadRxHashpipe::trivial_stream::addNextNodePointer(multi_node * nextNode){
    this->nextNodeNested=nextNode;
}

void SpeadRxHashpipe::trivial_stream::addPacketArmortiser(boost::shared_ptr<PacketArmortiser> outPacketArmortiser){
    this->outPacketArmortiser = outPacketArmortiser;
}


