//Global Defines
#define NUM_ANTENNAS 64
#define NUM_CHANNELS_PER_XENGINE 16 
#define NUM_POLLS 2
#define NUM_TIME_SAMPLES 256
#define QUADRANT_SIZE ((NUM_ANTENNAS/2+1)*(NUM_ANTENNAS/4))
#define NUM_BASELINES (QUADRANT_SIZE*4)

//Global Structs

//Struct storing samples as formatted bythe F-Engines
typedef struct DualPollComplexStruct_in {
  int8_t realPol0;
  int8_t imagPol0;
  int8_t realPol1;
  int8_t imagPol1;
} DualPollComplex_in;

//Struct of output baselines
#ifndef DP4A
typedef struct BaselineProductsStruct_out {
  float product0;
  float product1;
  float product2;
  float product3;
} BaselineProducts_out;
#else
typedef struct BaselineProductsStruct_out {
  int product0;
  int product1;
  int product2;
  int product3;
} BaselineProducts_out;
#endif

//Struct being fed to xGPU
typedef struct XGpuPacketInStruct{
    uint64_t timestamp_u64;
    uint64_t fEnginesPresent_u64;
    uint64_t frequencyBase_u64;
    uint8_t numFenginePacketsProcessed;
    DualPollComplex_in * samples_s;//[NUM_CHANNELS_PER_XENGINE*NUM_ANTENNAS*NUM_TIME_SAMPLES];
} XEnginePacketIn;
