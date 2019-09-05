#include "Spead2Rx.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#define NUM_THREADS 1

Spead2Rx::Spead2Rx(multi_node * nextNode, int rxPort): worker(NUM_THREADS),stream(worker),n_complete(0),endpoint(boost::asio::ip::address_v4::any(), rxPort),nextNode(nextNode){
    stream.addNextNodePointer(nextNode);
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
    outPacketArmortiser = boost::make_shared<Spead2RxPacketWrapper>();
    stream.addPacketArmortiser(outPacketArmortiser);
}

boost::shared_ptr<StreamObject> Spead2Rx::process_heap(boost::shared_ptr<spead2::recv::heap> fheap){
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
        return boost::make_shared<Spead2RxPacket>(timestamp,false,frequency,fengId,payloadPtr_p,fheap);
    }else{
        std::cout << "Unidentified Packet Received. This may be a broken F-Engine packet" << std::endl;
        return boost::shared_ptr<Spead2RxPacket>(nullptr);
    }
}

int Spead2Rx::getNumCompletePackets(){
  return n_complete;
}

void Spead2Rx::trivial_stream::heap_ready(spead2::recv::live_heap &&heap)
{
    pipelineCounts.heapsReceived++;
    if (heap.is_complete())
    {   
        boost::shared_ptr<StreamObject> spead2RxPacket = process_heap(boost::make_shared<spead2::recv::heap>(boost::move(heap)));
        if(spead2RxPacket!=nullptr){
          if(spead2RxPacket->isEOS()){
            std::cout << "End of stream packet received"<< std::endl;
          }else{
            pipelineCounts.Spead2RxStage++;
            outPacketArmortiser->addPacket(spead2RxPacket);
            if(outPacketArmortiser->getArmortiserSize() >= NUM_ANTENNAS*ARMORTISER_SIZE){
              if(!nextNodeNested->try_put(outPacketArmortiser)){
                std::cout << "Packet Failed to be passed to Buffer class" << std::endl;
              }
              outPacketArmortiser = boost::make_shared<Spead2RxPacketWrapper>();
            }
          }
        }
    }else{
      pipelineCounts.heapsDropped++;
    }
}


void Spead2Rx::trivial_stream::stop_received()
{
    spead2::recv::stream::stop_received();
    stop_promise.set_value();
}

void Spead2Rx::trivial_stream::join()
{
    std::future<void> future = stop_promise.get_future();
    future.get();
}

void Spead2Rx::trivial_stream::addNextNodePointer(multi_node * nextNode){
    this->nextNodeNested=nextNode;
}

void Spead2Rx::trivial_stream::addPacketArmortiser(boost::shared_ptr<Spead2RxPacketWrapper> outPacketArmortiser){
    this->outPacketArmortiser = outPacketArmortiser;
}
