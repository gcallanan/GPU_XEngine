#include "Spead2Rx.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>



Spead2Rx::Spead2Rx(int rxPort): worker(NUM_SPEAD2_RX_THREADS),stream(worker),n_complete(0),endpoint(boost::asio::ip::address_v4::any(), rxPort){
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
}

// boost::shared_ptr<StreamObject> Spead2Rx::process_heap(boost::shared_ptr<spead2::recv::heap> fheap){
//     const auto &items = fheap->get_items();
//     uint64_t fengId;
//     uint64_t timestamp;
//     uint64_t frequency;
//     uint8_t * payloadPtr_p;
//     bool fengPacket = false;
    
//     for (const auto &item : items)
//     {
//         if(item.id == 0x1600){
//           timestamp = item.immediate_value;
//         }

//         if(item.id == 0x4101){
//           fengId = item.immediate_value;
//         }

//         if(item.id == 0x4103){
//           frequency = item.immediate_value;
//         }

//         if(item.id == 0x4300){
//           payloadPtr_p = item.ptr;
//           fengPacket = true;
//         }
//     }

//     if(fengPacket){
//         return boost::make_shared<Spead2RxPacket>(timestamp,false,frequency,fengId,payloadPtr_p,fheap);
//     }else{
//         std::cout << "Unidentified Packet Received. This may be a broken F-Engine packet" << std::endl;
//         return boost::shared_ptr<Spead2RxPacket>(nullptr);
//     }
// }

int Spead2Rx::getNumCompletePackets(){
  return n_complete;
}

void Spead2Rx::trivial_stream::heap_ready(spead2::recv::live_heap &&heap)
{
    pipelineCounts.heapsReceived++;
    if (heap.is_complete())
    {   
      #if NUM_SPEAD2_RX_THREADS > 1
        mutex.lock();
      #endif
      pipelineCounts.Spead2RxStage++;
      #if NUM_SPEAD2_RX_THREADS > 1
        mutex.unlock();
      #endif
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

