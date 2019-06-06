#include "Spead2Rx.h"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>


Spead2Rx::Spead2Rx(multi_node * nextNode): worker(),stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2),n_complete(0),endpoint(boost::asio::ip::address_v4::any(), 8888),nextNode(nextNode){
    stream.addNextNodePointer(nextNode);
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
}

/*boost::shared_ptr<StreamObject> Spead2Rx::receive_packet(){
    try{
        boost::shared_ptr<spead2::recv::heap> fh = boost::make_shared<spead2::recv::heap>(stream.pop());
        n_complete++;
        pipelineCounts.Spead2Stage++;
        return process_heap(fh);
    }
    catch (spead2::ringbuffer_stopped &e){
        return boost::make_shared<StreamObject>(true);
    }
}*/

boost::shared_ptr<StreamObject> Spead2Rx::process_heap(boost::shared_ptr<spead2::recv::heap> fheap){
    const auto &items = fheap->get_items();
    uint64_t fengId;
    uint64_t timestamp;
    uint64_t frequency;
    uint8_t * payloadPtr_p;
    bool fengPacket = false;
    
    for (const auto &item : items)
    {
        //std::cout << "    ID: 0x" << std::hex << item.id << std::dec << ' ';
        //std::cout << "[" << item.length << " bytes]";
        //std::cout << '\n';
        if(item.id == 0x1600){
          timestamp = item.immediate_value;
          //std::cout<<timestamp<<std::endl;
        }

        if(item.id == 0x4101){
          fengId = item.immediate_value;
          //std::cout<<fengId<<std::endl;
        }

        if(item.id == 0x4103){
          frequency = item.immediate_value;
          //std::cout<<frequency<<std::endl;
        }

        if(item.id == 0x4300){
          payloadPtr_p = item.ptr;//(DualPollComplex *)item.ptr;
          fengPacket = true;
          //for (size_t i = 0; i < 16*256*2*2; i++)
          //{
          //  std::cout<<" "<<i<<" "<<(int)*(payloadPtr_p+i) << std::endl;;
          //}
          //std::cout << item.is_immediate << " " << item.length << " " << (uint64_t)item.ptr << std::endl;
        }
    }

    if(fengPacket){
        //std::cout<<"Spead2Rx: "<<timestamp<<" "<<frequency<<" "<<fengId<<std::endl;
        if(fengId > 63){
          std::cout << "Lol what" << std::endl;
        }
        return boost::make_shared<Spead2RxPacket>(timestamp,false,frequency,fengId,payloadPtr_p,fheap);
    }else{
        return boost::shared_ptr<Spead2RxPacket>(nullptr);
    }
}

int Spead2Rx::getNumCompletePackets(){
  return n_complete;
}

void Spead2Rx::trivial_stream::heap_ready(spead2::recv::live_heap &&heap)
{
    //std::cout << "Got heap " << heap.get_cnt();
    if (heap.is_complete())
    {   
        boost::shared_ptr<StreamObject> spead2RxPacket = process_heap(boost::make_shared<spead2::recv::heap>(boost::move(heap)));
        if(spead2RxPacket!=nullptr){
          if(spead2RxPacket->isEOS()){
            std::cout << "End of stream"<< std::endl;
            throw "End of Spead stream";
          }
          pipelineCounts.Spead2Stage++;
          //std::cout << "asd" << std::endl;
          if(!nextNodeNested->try_put(spead2RxPacket)){
            std::cout << "Packet Failed to be passed to buffer class" << std::endl;
          }
        }
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
