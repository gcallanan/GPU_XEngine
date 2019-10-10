#include "SpeadRx.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>



SpeadRx::SpeadRx(multi_node * nextNode, int rxPort): worker(NUM_SPEAD2_RX_THREADS),stream(worker),n_complete(0),endpoint(boost::asio::ip::address_v4::any(), rxPort),nextNode(nextNode){
    stream.addNextNodePointer(nextNode);
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
    outPacketArmortiser = boost::make_shared<PacketArmortiser>();
    stream.addPacketArmortiser(outPacketArmortiser);
}

boost::shared_ptr<PipelinePacket> SpeadRx::process_heap(boost::shared_ptr<spead2::recv::heap> fheap){
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

int SpeadRx::getNumCompletePackets(){
  return n_complete;
}

void SpeadRx::trivial_stream::heap_ready(spead2::recv::live_heap &&heap)
{
    pipelineCounts.heapsReceived++;
    if (heap.is_complete())
    {   
        boost::shared_ptr<PipelinePacket> SpeadRxPacket = process_heap(boost::make_shared<spead2::recv::heap>(boost::move(heap)));
        if(SpeadRxPacket!=nullptr){
          if(SpeadRxPacket->isEOS()){
            std::cout << "End of stream packet received"<< std::endl;
          }else{
            #if NUM_SPEAD2_RX_THREADS > 1
              mutex.lock();
            #endif
            pipelineCounts.SpeadRxStage++;
            outPacketArmortiser->addPacket(SpeadRxPacket);
            if(outPacketArmortiser->getArmortiserSize() >= NUM_ANTENNAS*ARMORTISER_SIZE){
              if(!nextNodeNested->try_put(outPacketArmortiser)){
                std::cout << "Packet Failed to be passed to Buffer class" << std::endl;
              }
              outPacketArmortiser = boost::make_shared<PacketArmortiser>();
            }
            #if NUM_SPEAD2_RX_THREADS > 1
              mutex.unlock();
            #endif
          }
        }
    }else{
      pipelineCounts.heapsDropped++;
    }
}


void SpeadRx::trivial_stream::stop_received()
{
    spead2::recv::stream::stop_received();
    stop_promise.set_value();
}

void SpeadRx::trivial_stream::join()
{
    std::future<void> future = stop_promise.get_future();
    future.get();
}

void SpeadRx::trivial_stream::addNextNodePointer(multi_node * nextNode){
    this->nextNodeNested=nextNode;
}

void SpeadRx::trivial_stream::addPacketArmortiser(boost::shared_ptr<PacketArmortiser> outPacketArmortiser){
    this->outPacketArmortiser = outPacketArmortiser;
}
