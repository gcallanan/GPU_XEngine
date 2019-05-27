#include <iostream> 
#include <cuda_runtime.h>
#include <math.h>
#include "kernels.h"
//#include <spead2/common_thread_pool.h>
//#include <spead2/recv_udp.h>
//#include <spead2/recv_heap.h>
//#include <spead2/recv_live_heap.h>
//#include <spead2/recv_ring_stream.h>
//#include <spead2/recv_udp_pcap.h>

int main(void)
{
  int N = 1<<20;
  float *x, *y;

  std::cout << "Cuda Compile Test" << std::endl;

  // Allocate Unified Memory – accessible from CPU or GPU

  std::cout << "Allocating Unified Memory Buffers" << std::endl;
  cudaMallocManaged(&x, N*sizeof(float));
  cudaMallocManaged(&y, N*sizeof(float));

  // initialize x and y arrays on the host
  std::cout << "Populating Unified Memory Buffers" << std::endl;
  for (int i = 0; i < N; i++) {
    x[i] = 1.0f;
    y[i] = 2.0f;
  }

  // Run kernel on 1M elements on the GPU

  std::cout << "Executing cuda kernel to add two buffers" << std::endl;
  add(N, x, y);

  // Wait for GPU to finish before accessing on host
  cudaDeviceSynchronize();

  // Check for errors (all values should be 3.0f)
  std::cout << "Confirming Correctness" << std::endl;
  float maxError = 0.0f;
  for (int i = 0; i < N; i++)
    maxError = fmax(maxError, fabs(y[i]-3.0f));
  std::cout << "Max error: " << maxError << std::endl;

  // Free memory
  
  std::cout << "Freeing Unified Memory Buffers" << std::endl;
  cudaFree(x);
  cudaFree(y);
  
  return 0;
}
