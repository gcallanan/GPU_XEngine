#include "Spead2Rx.h"

Spead2Rx::Spead2Rx(): worker(),stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2),n_complete(0),endpoint(boost::asio::ip::address_v4::any(), 8888){
    pool = std::make_shared<spead2::memory_pool>(262144, 26214400, 64, 64);
    stream.set_memory_allocator(pool);
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);
}

std::shared_ptr<StreamObject> Spead2Rx::receive_packet(){
    try{
        spead2::recv::heap fh = stream.pop();
        n_complete++;
        return process_heap(fh);
    }
    catch (spead2::ringbuffer_stopped &e){
        return std::make_shared<StreamObject>(true);
    }
}

std::shared_ptr<StreamObject> Spead2Rx::process_heap(spead2::recv::heap &fheap){
    const auto &items = fheap.get_items();
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
        return std::make_shared<Spead2RxPacket>(timestamp,false,frequency,fengId,payloadPtr_p,fheap);
    }else{
        return std::shared_ptr<Spead2RxPacket>(nullptr);
    }
}

