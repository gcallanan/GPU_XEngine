CUDA_DIR ?= /usr/local/cuda
NVCC = $(CUDA_DIR)/bin/nvcc
DEPENDANCIES = spead_test.o

spead_test.out: $(DEPENDANCIES)
	g++ -std=c++11 -o $@ $^ -lboost_system -lspead2 -pthread -lpcap 
#-Xcompiler="-pthread"

%.o: %.cpp
	g++ -std=gnu++11 -I /usr/include/boost/ -c -o $@ $^
 
clean:
	rm *.o
	rm *.out
    
