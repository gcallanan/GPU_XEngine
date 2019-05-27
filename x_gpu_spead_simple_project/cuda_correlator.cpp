#include <iostream>
#include <fstream>
#include <utility>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
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
#include <math.h>


//Defines
#define NUM_ANTENNAS 64
#define NUM_CHANNELS_PER_XENGINE 16 
#define NUM_POLLS 2
#define NUM_TIME_SAMPLES 256
#define QUADRANT_SIZE ((NUM_ANTENNAS/2+1)*(NUM_ANTENNAS/4))
#define NUM_BASELINES QUADRANT_SIZE*4

//Type Definitions
typedef struct DualPollComplexStruct_i8 {
  int8_t realPol0;
  int8_t imagPol0;
  int8_t realPol1;
  int8_t imagPol1;
} DualPollComplex_i8;

#ifndef DP4A
typedef struct DualPollComplexStruct_i32 {
  float p1;
  float p2;
  float p3;
  float p4;
} DualPollComplex_i32;
#else
typedef struct DualPollComplexStruct_i32 {
  int p1;
  int p2;
  int p3;
  int p4;
} DualPollComplex_i32;
#endif

typedef struct xFpgaComplexStruct {
  int32_t real;
  int32_t imag;
} xFpgaComplex;

typedef struct XEnginePacketInStruct{
    uint64_t timestamp_u64;
    uint64_t fEnginesPresent_u64;
    uint64_t frequencyBase_u64;
    uint8_t numFenginePacketsProcessed;
    DualPollComplex_i8 * samples_s;//[NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS*NUM_TIME_SAMPLES];
} XEnginePacketIn;

typedef struct XEnginePacketOutStruct{
    uint64_t timestamp_u64;
    uint64_t frequencyBase_u64;
    xFpgaComplex samples_s[NUM_CHANNELS_PER_XENGINE*NUM_BASELINES];
} XEnginePacketOut;
 
int xEngCount = 0;
uint64_t xEnginesPresent = 0ULL;
XEnginePacketIn xEnginePacketInTemp = {0,0,0};
XEnginePacketOut xEnginePacketOutTemp = {0,0};
//DualPollComplex * FEnginePacketOut_p[NUM_CHANNELS_PER_XENGINE*NUM_TIME_SAMPLES];

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

//Static Variables
static time_point start = std::chrono::high_resolution_clock::now();
static std::uint64_t n_complete = 0;

#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int clock_gettime(int clk_id, struct timespec *t){
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
}
#else
#include <time.h>
#endif

#include "cube/cube.h"
#include "xgpu.h"

/*
  Data ordering for input vectors is (running from slowest to fastest)
  [time][channel][station][polarization][complexity]

  Output matrix has ordering
  [channel][station][station][polarization][polarization][complexity]
*/

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
    uint64_t timestamp;
    uint64_t frequency;
    uint8_t * payloadPtr_p;
    DualPollComplex_i8 * FEnginePacketOut_p;
    xFpgaComplex * XEnginePacketOut_p;

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

        if(item.id == 0x1800){
          payloadPtr_p = item.ptr;
          xengPacket = true;
        }
    }
    
    //57882838040576
    //57878484353024
    //57880661196800
    if(xengPacket && timestamp == 40968192){
      correctTimestamp = true;
      std::cout << "X-Engine Timestamp: " << timestamp << std::endl;
    }

    if(fengPacket && timestamp == 40968192){
      correctTimestamp = true;
      //std::cout << "F-Engine Timestamp: " << timestamp << std::endl;
    }

    if(correctTimestamp && fengPacket){
        FEnginePacketOut_p = (DualPollComplex_i8*) payloadPtr_p;
        xEnginePacketInTemp.numFenginePacketsProcessed+=1;
        xEnginePacketInTemp.fEnginesPresent_u64 |= ((uint64_t)1<<fengId);
        xEnginePacketInTemp.frequencyBase_u64 = frequency;
        xEnginePacketInTemp.timestamp_u64 = timestamp;
        for(int channel_index = 0; channel_index < NUM_CHANNELS_PER_XENGINE; channel_index++)
        {
            for(int time_index = 0; time_index < NUM_TIME_SAMPLES; time_index++)
            {
                DualPollComplex_i8 * inputSample = &FEnginePacketOut_p[channel_index*NUM_TIME_SAMPLES + time_index];
                xEnginePacketInTemp.samples_s[time_index*NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS+channel_index*NUM_ANTENNAS+fengId] = *inputSample;
                //std::cout<<time_index << " "<< channel_index << " "<< (time_index*NUM_CHANNELS_PER_XENGINE+channel_index*NUM_ANTENNAS+fengId) << " " << (int)inputSample->imagPol0 << " " << (int)inputSample->imagPol1 << " " << (int)inputSample->realPol0 << " " << (int)inputSample->realPol1 << " " << (int)xEnginePacketInTemp.samples_s[time_index*NUM_CHANNELS_PER_XENGINE+channel_index*NUM_ANTENNAS+fengId].imagPol0 << " " << (int)xEnginePacketInTemp.samples_s[time_index*NUM_CHANNELS_PER_XENGINE+channel_index*NUM_ANTENNAS+fengId].imagPol1 << " " << (int)xEnginePacketInTemp.samples_s[time_index*NUM_CHANNELS_PER_XENGINE+channel_index*NUM_ANTENNAS+fengId].realPol0 << " " << (int)xEnginePacketInTemp.samples_s[time_index*NUM_CHANNELS_PER_XENGINE+channel_index*NUM_ANTENNAS+fengId].realPol1 << std::endl;
            }
            
        }
        std::cout << std::setfill('0') << std::setw(2) << int(fengId) <<" "<< std::setfill('0') << std::setw(2) <<int(xEnginePacketInTemp.numFenginePacketsProcessed)<< " " << std::bitset<64>(xEnginePacketInTemp.fEnginesPresent_u64) << " " <<  std::endl;
    }

    if(correctTimestamp && xengPacket){
        //std::cout << sizeof(xEnginePacketOutTemp) << std::endl;
        XEnginePacketOut_p = (xFpgaComplex*) payloadPtr_p;
        xEnginePacketOutTemp.frequencyBase_u64 = frequency;
        xEnginePacketOutTemp.timestamp_u64 = timestamp;
        for(size_t i = 0; i < NUM_CHANNELS_PER_XENGINE*NUM_BASELINES; i++)
        {
          xEnginePacketOutTemp.samples_s[i] = XEnginePacketOut_p[i];
        }
        
    }


    std::vector<spead2::descriptor> descriptors = fheap.get_descriptors();
    for (const auto &descriptor : descriptors)
    {
//        std::cout
//            << "    0x" << std::hex << descriptor.id << std::dec << ":\n"
//            << "        NAME:  " << descriptor.name << "\n"
//            << "        DESC:  " << descriptor.description << "\n";
        if (descriptor.numpy_header.empty())
        {
//            std::cout << "        TYPE:  ";
//            for (const auto &field : descriptor.format)
//                std::cout << field.first << field.second << ",";
//            std::cout << "\n";
//            std::cout << "        SHAPE: ";
//            for (const auto &size : descriptor.shape)
//                if (size == -1)
//                    std::cout << "?,";
//                else
//                    std::cout << size << ",";
//            std::cout << "\n";
        }
//        else
//            std::cout << "        DTYPE: " << descriptor.numpy_header << "\n";
    }
    time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - start;
    //std::cout << elapsed.count() << "\n";
//    std::cout << std::flush;
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
    std::shared_ptr<spead2::memory_pool> pool = std::make_shared<spead2::memory_pool>(262144, 26214400, 64, 64);
    spead2::recv::ring_stream<> stream(worker, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
    stream.set_memory_allocator(pool);
    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), 8888);
     //4k
    //std::string filename = std::string("/home/kat/capture_fengine_data/2019-03-15/gcallanan_feng_capture2019-03-15-08:43:22.pcap");//1k
    //stream.emplace_reader<spead2::recv::udp_pcap_file_reader>(fileName);
    stream.emplace_reader<spead2::recv::udp_reader>(endpoint, spead2::recv::udp_reader::default_max_size, 8 * 1024 * 1024);

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

int getBaselineOffset(int ant0, int ant1){
    if(ant0>ant1)
      throw "Condition a0<=a1 does not hold";
    int quadrant = 2*(ant0&1) + (ant1&1);
    int quadrant_index = (ant1/2)*(ant1/2 + 1)/2 + ant0/2;
    return quadrant*(QUADRANT_SIZE) + quadrant_index;
}

void displayBaseline(DualPollComplexStruct_i32* xEnginePacketOut, int i, int j){
    //int startOffset = getBaselineOffset(i,j);
    for (int k = 0; k < NUM_CHANNELS_PER_XENGINE; k++)
    {
      //if(xEnginePacketOut[k].p1 != 0 || xEnginePacketOut[k].p2 != 0 || xEnginePacketOut[k].p3 != 0|| xEnginePacketOut[k].p4 != 0){ //|| xEnginePacketOut[k].p5 != 0 || xEnginePacketOut[k].p6 != 0 || xEnginePacketOut[k].p7 != 0|| xEnginePacketOut[k].p8 != 0){
      //  std::cout << k 
      //      << " " << xEnginePacketOut[k].p1 << " " << xEnginePacketOut[k].p2
      //      << " " << xEnginePacketOut[k].p3 << " " << xEnginePacketOut[k].p4
      //      << std::endl;
      //}
      //int baseline_offset = k*NUM_ANTENNAS*(NUM_ANTENNAS/2+1)/2 + i*(i+1)/2+j;
      int index = k*(QUADRANT_SIZE*4)+getBaselineOffset(i,j);
      std::cout<< "Real: "<< i << " " << j << " " << k << " " << index << " " 
          << " "<< xEnginePacketOut[index].p1 << " " << xEnginePacketOut[index].p2
          << " "<< xEnginePacketOut[index].p3 << " " << xEnginePacketOut[index].p4
          << std::endl;
      index = index + NUM_BASELINES*NUM_CHANNELS_PER_XENGINE;
      std::cout<<"Imag: " << i << " " << j << " " << k << " " << index << " " 
          << " "<< xEnginePacketOut[index].p1 << " " << xEnginePacketOut[index].p2
          << " "<< xEnginePacketOut[index].p3 << " " << xEnginePacketOut[index].p4
          << std::endl;
    
    }
    
    
}

int main(int argc, char** argv) {
//*********************************SPEAD TEST********************************

    //filename_in = std::string("/home/kat/capture_fengine_data/2019-04-16/gcallanan_feng_capture_2019-04-16-05:46:11_4s.pcap");
    //run_ringbuffered(filename_in);
    //std::cout << "Reading from file: " << filename_in << std::endl;
    //std::cout << "Received " << n_complete << " complete F-Engine heaps\n";

    //std::ofstream outfile;
    //outfile.open("/home/kat/capture_fengine_data/2019-04-16/xFpgaOut.txt", std::ios::out);
    //for(size_t i = 0; i < NUM_CHANNELS_PER_XENGINE; i++)
    //{
      //outfile << sqrt(((uint64_t)xEnginePacketOutTemp.samples_s[i*NUM_BASELINES].real * (uint64_t)xEnginePacketOutTemp.samples_s[i*NUM_BASELINES].real) + ((uint64_t)xEnginePacketOutTemp.samples_s[i*NUM_BASELINES].imag * (uint64_t)xEnginePacketOutTemp.samples_s[i*NUM_BASELINES].imag)) << std::endl;
    //  double angle = atan2(xEnginePacketOutTemp.samples_s[i*NUM_BASELINES].imag,xEnginePacketOutTemp.samples_s[i*NUM_BASELINES].real);
    //  double power = sqrt(((int64_t)xEnginePacketOutTemp.samples_s[i].real * (uint64_t)xEnginePacketOutTemp.samples_s[i].real) + ((uint64_t)xEnginePacketOutTemp.samples_s[i].imag * (uint64_t)xEnginePacketOutTemp.samples_s[i].imag));
      //angle = atan2(100,110);
    //  if(angle > 3.14){
    //    angle-=3.14;
    //  }
    //  outfile << power << " " << angle <<std::endl;
    //}
    //for(size_t i = 0; i < NUM_BASELINES; i++)
    //{
    //  outfile << sqrt(((int64_t)xEnginePacketOutTemp.samples_s[i].real * (uint64_t)xEnginePacketOutTemp.samples_s[i].real) + ((uint64_t)xEnginePacketOutTemp.samples_s[i].imag * (uint64_t)xEnginePacketOutTemp.samples_s[i].imag)) << std::endl;
    //}
    //outfile.close();
    //********************************END SPEAD TEST*****************************
  //for (int i = 0; i < NUM_CHANNELS_PER_XENGINE*NUM_TIME_SAMPLES; i++)
  //{
  //    std::cout<<(int)xEnginePacketInTemp.samples_s[i].realPol0 << " " <<(int)xEnginePacketInTemp.samples_s[i].imagPol0 << " " <<(int)xEnginePacketInTemp.samples_s[i].realPol1 << " " <<(int)xEnginePacketInTemp.samples_s[i].imagPol1 << " " <<std::endl;
  //}

  //TODO: This bit  

  int opt;
  int i, j;
  int device = 0;
  unsigned int seed = 1;
  int outer_count = 1;
  int count = 1;
  int syncOp = SYNCOP_SYNC_TRANSFER;
  int finalSyncOp = SYNCOP_DUMP;
  int verbose = 0;
  int hostAlloc = 1;
  XGPUInfo xgpu_info;
  unsigned int npol, nstation, nfrequency;
  int xgpu_error = 0;
  Complex *omp_matrix_h = NULL;
  struct timespec outer_start, start, stop, outer_stop;
  double total, per_call, max_bw, gbps;
#ifdef RUNTIME_STATS
  struct timespec tic, toc;
#endif

  while ((opt = getopt(argc, argv, "C:c:d:f:ho:rs:v:")) != -1) {
    switch (opt) {
      case 'c':
        // Set number of time to call xgpuCudaXengine
        count = strtoul(optarg, NULL, 0);
        if(count < 1) {
          fprintf(stderr, "count must be positive\n");
          return 1;
        }
        break;
      case 'C':
        // Set number of time to call xgpuCudaXengine
        outer_count = strtoul(optarg, NULL, 0);
        if(outer_count < 1) {
          fprintf(stderr, "outer count must be positive\n");
          return 1;
        }
        break;
      case 'd':
        // Set CUDA device number
        device = strtoul(optarg, NULL, 0);
        break;
      case 'f':
        // Set syncOp for final call
        finalSyncOp = strtoul(optarg, NULL, 0);
        break;
      case 'o':
        // Set syncOp
        syncOp = strtoul(optarg, NULL, 0);
        break;
      case 'r':
        // Register host allocated memory
        hostAlloc = 1;
        break;
      case 's':
        // Set seed for random data
        seed = strtoul(optarg, NULL, 0);
        break;
      case 'v':
        // Set verbosity level
        verbose = strtoul(optarg, NULL, 0);
        break;
      default: /* '?' */
        fprintf(stderr,
            "Usage: %s [options]\n"
            "Options:\n"
            "  -c INTEG_CALLS    Calls to xgpuCudaXengine per integration [1]\n"
            "  -C INTEG_COUNT    Number of integrations [1]\n"
            "  -d DEVNUM         GPU device to use [0]\n"
            "  -f FINAL_SYNCOP   Sync operation for final call [1]\n"
            "  -o SYNCOP         Sync operation for all but final call [1]\n"
            "                    Sync operation values are:\n"
            "                         0 (no sync)\n"
            "                         1 (sync and dump)\n"
            "                         2 (sync host to device transfer)\n"
            "                         3 (sync kernel computations)\n"
            "  -r                Register host allocated memory [false]\n"
            "                    (otherwise use CUDA allocated memory)\n"
            "  -s SEED           Random number seed [1]\n"
            "  -v {0|1|2|3}      Verbosity level (debug only) [0]\n"
            "  -h                Show this message\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  srand(seed);

  // Get sizing info from library
  xgpuInfo(&xgpu_info);
  npol = xgpu_info.npol;
  nstation = xgpu_info.nstation;
  nfrequency = xgpu_info.nfrequency;

  printf("Correlating %u stations with %u channels and integration length %u\n",
	 xgpu_info.nstation, xgpu_info.nfrequency, xgpu_info.ntime);
#ifndef FIXED_POINT
  printf("Sending floating point data to GPU.\n");
#else
  printf("Sending fixed point data to GPU.\n");
#endif
  printf("Complex Sample Size: %u bytes \n", sizeof(Complex));

  // perform host memory allocation

  // allocate the GPU X-engine memory
  XGPUContext context;

  if(hostAlloc) {
    context.array_len = xgpu_info.vecLength;
    context.matrix_len = xgpu_info.matLength;
    context.array_h = (ComplexInput*)malloc(context.array_len*sizeof(ComplexInput));//TODO: Change it so that ingest data is the data that is read
    context.matrix_h = (Complex*)malloc(context.matrix_len*sizeof(Complex));
  } else {
    context.array_h = NULL;
    context.matrix_h = NULL;
  }
 
  #ifndef DP4A
  ComplexInput *array_h = context.array_h; // this is pinned memory
  #else
  ComplexInput *array_h = (ComplexInput *)malloc(context.array_len*sizeof(ComplexInput));
  #endif

  std::cout << "XGPU Test" << std::endl;
  std::cout << "Waiting to receive FEngine packets" << std::endl;
  xEnginePacketInTemp.samples_s = (DualPollComplex_i8*)array_h;
  //std::cout << (int)xEnginePacketInTemp.samples_s[xgpu_info.vecLength-1].imagPol0 << std::endl;
  //memset(&xEnginePacketInTemp,0,sizeof(xEnginePacketInTemp));
  std::string filename_in = std::string("/home/kat/capture_fengine_data/2019-04-16/gcallanan_xeng_capture_2019-04-16-05:46:08_4s.pcap");
  run_ringbuffered(filename_in);
  std::cout << "Reading from file: " << filename_in << std::endl;
  std::cout << "Received " << n_complete << " complete X-Engine heaps\n";


  printf("xGPU Array Size %i, xFpga Arraylength %i\n",xgpu_info.vecLength*sizeof(ComplexInput),sizeof(xEnginePacketInTemp.samples_s));
  //for (int i = 0; i < xgpu_info.vecLength/2; i++)
  //{
  //  std::cout << i << " " << (int)xEnginePacketInTemp.samples_s[i].imagPol0 <<" "<< (int)xEnginePacketInTemp.samples_s[i].imagPol1 <<" "<< (int)xEnginePacketInTemp.samples_s[i].realPol0 <<" "<< (int)xEnginePacketInTemp.samples_s[i].realPol1 << std::endl;
  //}

  xgpu_error = xgpuInit(&context, device);
  if(xgpu_error) {
    fprintf(stderr, "xgpuInit returned error code %d\n", xgpu_error);
    return -1;
    //goto cleanup;
  }


  Complex *cuda_matrix_h = context.matrix_h;

  // create an array of complex noise
  //xgpuRandomComplex(array_h, xgpu_info.vecLength);

#ifdef DP4A
  xgpuSwizzleInput(context.array_h, array_h);
#endif

  // ompXengine always uses TRIANGULAR_ORDER
  unsigned int ompMatLength = nfrequency * ((nstation+1)*(nstation/2)*npol*npol);
  omp_matrix_h = (Complex *) malloc(ompMatLength*sizeof(Complex));
  if(!omp_matrix_h) {
    fprintf(stderr, "error allocating output buffer for xgpuOmpXengine\n");
    //goto cleanup;
  }

#if (CUBE_MODE == CUBE_DEFAULT && !defined(POWER_LOOP) )
  // Only call CPU X engine if dumping GPU X engine exactly once
  if(finalSyncOp == SYNCOP_DUMP && count*outer_count == 1) {
    printf("Calling CPU X-Engine\n");
    xgpuOmpXengine(omp_matrix_h, array_h);
  }
#endif

#define ELAPSED_MS(start,stop) \
  ((((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))/1e6)

  printf("Calling GPU X-Engine\n");
  clock_gettime(CLOCK_MONOTONIC, &outer_start);
  for(j=0; j<outer_count; j++) {
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(i=0; i<count; i++) {
#ifdef RUNTIME_STATS
      clock_gettime(CLOCK_MONOTONIC, &tic);
#endif
      xgpu_error = xgpuCudaXengine(&context, i==count-1 ? finalSyncOp : syncOp);
#ifdef RUNTIME_STATS
      clock_gettime(CLOCK_MONOTONIC, &toc);
#endif
      if(xgpu_error) {
        fprintf(stderr, "xgpuCudaXengine returned error code %d\n", xgpu_error);
        //goto cleanup;
      }
#ifdef RUNTIME_STATS
      fprintf(stderr, "%11.6f  %11.6f ms%s\n",
          ELAPSED_MS(start,tic), ELAPSED_MS(tic,toc),
          i==count-1 ? " final" : "");
#endif
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    total = ELAPSED_MS(start,stop);
    per_call = total/count;
    // per_spectrum = per_call / NTIME
    // per_channel = per_spectrum / NFREQUENCY
    //             = per_call / (NTIME * NFREQUENCY)
    // max_bw (kHz)  = 1 / per_channel = (NTIME * NFREQUENCY) / per_call
    max_bw = xgpu_info.ntime*xgpu_info.nfrequency/per_call/1000; // MHz
    gbps = ((float)(8 * context.array_len * sizeof(ComplexInput) * count)) / total / 1e6; // Gbps
    printf("Elapsed time %.6f ms total, %.6f ms/call average\n",
        total, per_call);
    printf("Theoretical BW_max %.3f MHz, throughput %.3f Gbps\n",
        max_bw, gbps);
  }
  if(outer_count > 1) {
    clock_gettime(CLOCK_MONOTONIC, &outer_stop);
    total = ELAPSED_MS(outer_start,outer_stop);
    per_call = total/(count*outer_count);
    // per_spectrum = per_call / NTIME
    // per_channel = per_spectrum / NFREQUENCY
    //             = per_call / (NTIME * NFREQUENCY)
    // max_bw (kHz)  = 1 / per_channel = (NTIME * NFREQUENCY) / per_call
    max_bw = xgpu_info.ntime*xgpu_info.nfrequency/per_call/1000; // MHz
    gbps = ((float)(8 * context.array_len * sizeof(ComplexInput) * count * outer_count)) / total / 1e6; // Gbps
    printf("Elapsed time %.6f ms total, %.6f ms/call average\n",
        total, per_call);
    printf("Theoretical BW_max %.3f MHz, throughput %.3f Gbps\n",
        max_bw, gbps);
  }

displayBaseline((DualPollComplexStruct_i32*)cuda_matrix_h,16,58);

#if (CUBE_MODE == CUBE_DEFAULT)
  
  // Only compare CPU and GPU X engines if dumping GPU X engine exactly once
  if(finalSyncOp == SYNCOP_DUMP && count*outer_count == 1) {
    xgpuReorderMatrix(cuda_matrix_h);
    xgpuCheckResult(cuda_matrix_h, omp_matrix_h, verbose, array_h);
  }

  std::cout<<"Matrix Output Length: "<<context.matrix_len/2<<" samples of "<<sizeof(Complex)<<" bytes."<<std::endl;

#if 0
  int fullMatLength = nfrequency * nstation*nstation*npol*npol;
  Complex *full_matrix_h = (Complex *) malloc(fullMatLength*sizeof(Complex));

  // convert from packed triangular to full matrix
  xgpuExtractMatrix(full_matrix_h, cuda_matrix_h);

  free(full_matrix_h);
#endif
#endif

cleanup:
  //free host memory
  free(omp_matrix_h);

  // free gpu memory
  xgpuFree(&context);

#ifdef DP4A
  free(array_h);
#endif

  if(hostAlloc) {
    free(context.array_h);
    free(context.matrix_h);
  }

  return xgpu_error;
}
