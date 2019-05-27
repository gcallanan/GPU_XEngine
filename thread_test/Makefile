CUDA_DIR ?= /usr/local/cuda
NVCC = $(CUDA_DIR)/bin/nvcc
DEPENDANCIES = thread_test.o ThreadSafeQueue.o

thread_test.out: $(DEPENDANCIES)
	$(NVCC) -std=c++11 -o $@ $^ -lboost_system -lspead2 -Xcompiler -pthread -lpcap -ltbb
#-Xcompiler="-pthread"

%.o: %.cpp
	$(NVCC) -std=c++11 -c -o $@ $^

clean:
	rm *.o
	rm *.out
