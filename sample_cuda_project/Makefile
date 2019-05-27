CUDA_DIR ?= /usr/local/cuda
NVCC = $(CUDA_DIR)/bin/nvcc

DEBUG ?= -O3
CUDA_ARCH ?= sm_61

NVCCFLAGS = -Xcompiler -std=c++11 -Xcompiler -D_REENTRANT -Xcompiler -Wall -Xcompiler -Wno-unused-function -ftz=true
NVCCFLAGS += $(DEBUG) -arch=$(CUDA_ARCH) --ptxas-options=-v -prec-sqrt=false -Xcompiler -fPIC
#helloworld.run: helloworld.o
    
helloworld.out: kernels.o helloworld.o
	$(NVCC) $(NVCCFLAGS) -o $@ $^

helloworld.o : helloworld.cpp
	$(NVCC) $(NVCCFLAGS) -c -o $@ $^

kernels.o : kernels.cu
	$(NVCC) $(NVCCFLAGS) -c -o $@ $^

clean:
	rm *.out
	rm *.o
