#include "global_definitions.h"

PipelineCounts pipelineCounts = {};
bool debug = false;

int getBaselineOffset(int ant0, int ant1){
    if(ant0>ant1)
      throw "Condition a0<=a1 does not hold";
    int quadrant = 2*(ant0&1) + (ant1&1);
    int quadrant_index = (ant1/2)*(ant1/2 + 1)/2 + ant0/2;
    return quadrant*(QUADRANT_SIZE) + quadrant_index;
}

void displayBaseline(BaselineProducts_out* XGpuPacketOut, int i, int j){
    //int startOffset = getBaselineOffset(i,j);
    for (int k = 0; k < NUM_CHANNELS_PER_XENGINE; k++)
    {
      //if(XGpuPacketOut[k].p1 != 0 || XGpuPacketOut[k].p2 != 0 || XGpuPacketOut[k].p3 != 0|| XGpuPacketOut[k].p4 != 0){ //|| XGpuPacketOut[k].p5 != 0 || XGpuPacketOut[k].p6 != 0 || XGpuPacketOut[k].p7 != 0|| XGpuPacketOut[k].p8 != 0){
      //  std::cout << k 
      //      << " " << XGpuPacketOut[k].p1 << " " << XGpuPacketOut[k].p2
      //      << " " << XGpuPacketOut[k].p3 << " " << XGpuPacketOut[k].p4
      //      << std::endl;
      //}
      //int baseline_offset = k*NUM_ANTENNAS*(NUM_ANTENNAS/2+1)/2 + i*(i+1)/2+j;
      int index = k*NUM_BASELINES+getBaselineOffset(i,j);
      std::cout<< "Real: "<< i << " " << j << " " << k << " " << index << " " 
          << " "<< (XGpuPacketOut[index].product0/256.0/1600.0) << " " << (XGpuPacketOut[index].product1/256.0/1600.0)
          << " "<< (XGpuPacketOut[index].product2/256.0/1600.0) << " " << (XGpuPacketOut[index].product3/256.0/1600.0)
          << std::endl;
      index = index + NUM_BASELINES*NUM_CHANNELS_PER_XENGINE;
      std::cout<<"Imag: " << i << " " << j << " " << k << " " << index << " " 
          << " "<< (XGpuPacketOut[index].product0/256.0/1600.0) << " " << (XGpuPacketOut[index].product1/256.0/1600.0)
          << " "<< (XGpuPacketOut[index].product2/256.0/1600.0) << " " << (XGpuPacketOut[index].product3/256.0/1600.0)
          << std::endl;
    
    }
    
    
}
