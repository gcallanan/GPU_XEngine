//Includes
#include <iostream>
#include <utility>
#include <chrono>
#include <cstdint>
#include <boost/asio.hpp>
#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#include <spead2/recv_udp_pcap.h>
#include <spead2/recv_heap.h>
#include <spead2/recv_live_heap.h>
#include <spead2/recv_ring_stream.h>
#include <bitset>
#include <iomanip>

//Defines
#define NUM_ANTENNAS 64
#define NUM_CHANNELS_PER_XENGINE 16 
#define NUM_POLLS 2
#define NUM_TIME_SAMPLES 256

//Type Definitions
typedef struct DualPollComplexStruct {
  uint8_t realPol0;
  uint8_t imagPol0;
  uint8_t realPol1;
  uint8_t imagPol1;
} DualPollComplex;




typedef struct XEnginePacketInStruct{
    uint64_t timestamp_u64;
    uint64_t fEnginesPresent_u64;
    uint64_t frequencyBase_u64;
    uint8_t numFenginePacketsProcessed;
    DualPollComplex samples_s[NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS*NUM_TIME_SAMPLES];
} XEnginePacketIn;
 
int xEngCount = 0;
uint64_t xEnginesPresent = 0ULL;
XEnginePacketIn xEnginePacketInTemp = {0,0,0};
//DualPollComplex * FEnginePacketOut_p[NUM_CHANNELS_PER_XENGINE*NUM_TIME_SAMPLES];

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

//Static Variables
static time_point start = std::chrono::high_resolution_clock::now();
static std::uint64_t n_complete = 0;

class trivial_stream : public spead2::recv::stream
{
private:
    virtual void heap_ready(spead2::recv::live_heap &&heap) override
    {
        std::cout << "Got heap " << heap.get_cnt();
        if (heap.is_complete())
        {
            std::cout << " [complete]\n";
            n_complete++;
        }
        else if (heap.is_contiguous())
            std::cout << " [contiguous]\n";
        else
            std::cout << " [incomplete]\n";
    }

    std::promise<void> stop_promise;

public:
    using spead2::recv::stream::stream;
    
    virtual void stop_received() override
    {
        spead2::recv::stream::stop_received();
        stop_promise.set_value();
    }

    void join()
    {
        std::future<void> future = stop_promise.get_future();
        future.get();
    }
};

void show_heap(const spead2::recv::heap &fheap)
{
    //std::cout << "Received heap with CNT " << fheap.get_cnt() << '\n';
    const auto &items = fheap.get_items();
    //std::cout << items.size() << " item(s)\n";
    bool correctTimestamp = false;
    bool fengPacket = false;
    bool xengPacket = false;
    uint64_t fengId;
    DualPollComplex * FEnginePacketOut_p;


    for (const auto &item : items)
    {
        //std::cout << "    ID: 0x" << std::hex << item.id << std::dec << ' ';
        //std::cout << "[" << item.length << " bytes]";
        //std::cout << '\n';
        if(item.id == 0x1600){
            uint8_t timestamp_pc[8];
            if(8690677579776 == item.immediate_value || 57878484353024 == item.immediate_value || 57880661196800 == item.immediate_value){
                correctTimestamp = true;
            //    std::cout << "Timestamp: " << item.immediate_value << std::endl;//<< *timestamp_pi << std::endl;
            }
        }
        if(item.id == 0x4101){
            fengId = item.immediate_value;
        }
        if(item.id == 0x4103){
            //std::cout << "Frequency: " << item.immediate_value << std::endl;
        }
        if(item.id == 0x4300){
            FEnginePacketOut_p = (DualPollComplex *)item.ptr;
            fengPacket = true;
        }

        if(item.id == 0x1800){
            xengPacket = true;
        }
    }

    if(correctTimestamp && fengPacket){
        xEnginePacketInTemp.numFenginePacketsProcessed+=1;
        xEnginePacketInTemp.fEnginesPresent_u64 |= ((uint64_t)1<<fengId);
        for(size_t channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
        {
            for(size_t time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
            {
                DualPollComplex * inputSample = &FEnginePacketOut_p[channel_index*NUM_TIME_SAMPLES + time_index];
                xEnginePacketInTemp.samples_s[time_index*NUM_CHANNELS_PER_XENGINE+channel_index*NUM_ANTENNAS+fengId] = *inputSample;
            }
            
        }
        std::cout << std::setfill('0') << std::setw(2) << int(fengId) <<" "<< std::setfill('0') << std::setw(2) <<int(xEnginePacketInTemp.numFenginePacketsProcessed)<< " " << std::bitset<64>(xEnginePacketInTemp.fEnginesPresent_u64) << " " <<  std::endl;
    }


    std::vector<spead2::descriptor> descriptors = fheap.get_descriptors();
    for (const auto &descriptor : descriptors)
    {
        std::cout
            << "    0x" << std::hex << descriptor.id << std::dec << ":\n"
            << "        NAME:  " << descriptor.name << "\n"
            << "        DESC:  " << descriptor.description << "\n";
        if (descriptor.numpy_header.empty())
        {
            std::cout << "        TYPE:  ";
            for (const auto &field : descriptor.format)
                std::cout << field.first << field.second << ",";
            std::cout << "\n";
            std::cout << "        SHAPE: ";
            for (const auto &size : descriptor.shape)
                if (size == -1)
                    std::cout << "?,";
                else
                    std::cout << size << ",";
            std::cout << "\n";
        }
        else
            std::cout << "        DTYPE: " << descriptor.numpy_header << "\n";
    }
    time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - start;
    //std::cout << elapsed.count() << "\n";
    std::cout << std::flush;
}

static void run_trivial()
{
    spead2::thread_pool worker;
    trivial_stream stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), 8888);
    stream.emplace_reader<spead2::recv::udp_reader>(
        endpoint, spead2::recv::udp_reader::default_max_size, 1024 * 1024);
    stream.join();
}

static void run_ringbuffered(std::string fileName)
{
    spead2::thread_pool worker;
    std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(16384, 26214400, 12, 8);
    spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
    stream.set_memory_allocator(pool);
    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), 8888);
     //4k
    //std::string filename = std::string("/home/kat/capture_fengine_data/2019-03-15/gcallanan_feng_capture2019-03-15-08:43:22.pcap");//1k
    stream.emplace_reader<spead2::recv::udp_pcap_file_reader>(fileName);
    while (true)
    {
        try
        {
            spead2::recv::heap fh = stream.pop();
            n_complete++;
            show_heap(fh);
        }
        catch (spead2::ringbuffer_stopped &e)
        {
            break;
        }
    }
}

int main()
{
    // run_trivial();
    memset(&xEnginePacketInTemp,0,sizeof(xEnginePacketInTemp));
    std::string filename_in = std::string("/home/kat/capture_fengine_data/2019-04-16/gcallanan_xeng_capture_2019-04-16-05:46:08_4s.pcap");
    run_ringbuffered(filename_in);
    std::cout << "Reading from file: " << filename_in << std::endl;
    std::cout << "Received " << n_complete << " complete F-Engine heaps\n";
    n_complete = 0;
    return 0;
}

