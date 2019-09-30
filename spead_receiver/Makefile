OBJECT_FILES = spead_receiver.o global_definitions.o Spead2Rx.o

%.o: %.cpp
	g++ -std=c++11 -c -o $@ $<

spead_receiver: $(OBJECT_FILES)
	g++ -std=c++11 $(OBJECT_FILES)  -o $@ -lboost_system -lspead2 -pthread -lpcap -lboost_program_options

clean: 
	rm -f *.o 
	rm -f spead_receiver

	#/usr/local/cuda/bin/nvcc -std=c++11 -Xcompiler -D_REENTRANT -Xcompiler -Wall -Xcompiler -Wno-unused-function -ftz=true -Xcompiler -mavx512f  -Xcompiler -fopenmp -O3 -arch=sm_61 --ptxas-options=-v -prec-sqrt=false -Xcompiler -fPIC -DTEXTURE_DIM=1 -DNPOL=2 -DNSTATION=64 -DNFREQUENCY=16 -DNTIME=256 xgpuinfo.o -o xgpuinfo -L/usr/local/cuda/lib64 -L. -lrt -lboost_system -lspead2 -Xcompiler -lpthread -lpcap -lrdmacm -libverbs -ltbb -lboost_program_options -Xlinker -rpath,'${ORIGIN}:${ORIGIN}/../lib' -lxgpu