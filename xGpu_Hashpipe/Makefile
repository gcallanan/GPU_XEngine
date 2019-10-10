CC          = g++
HPDEMO_LIB_CCFLAGS     = -g -O3 -fPIC -shared -lstdc++ -mavx -msse4 -std=c++11 \
                     -I. -I$(CUDA_DIR)/include -I/usr/local/include  -I../xGpu_TBB \
                     -L. -L/usr/local/lib \
                     -lhashpipe -lrt -lm -lboost_system -lspead2
HPDEMO_LIB_TARGET   = xGpu_Hashpipe.so
HPDEMO_LIB_SOURCES  = xGpu_Hashpipe_databuf.cpp \
                      xGpu_Hashpipe_input_thread.cpp \
					  xGpu_Hashpipe_proc_thread.cpp \
					  xGpu_Hashpipe_end_thread.cpp \
					  ../xGpu_TBB/PipelinePackets.h
HPDEMO_LIB_INCLUDES = xGpu_Hashpipe_databuf.h

all: $(HPDEMO_LIB_TARGET)

$(HPDEMO_LIB_TARGET): $(HPDEMO_LIB_SOURCES) $(HPDEMO_LIB_INCLUDES)
	$(CC) -o $(HPDEMO_LIB_TARGET) $(HPDEMO_LIB_SOURCES) $(HPDEMO_LIB_CCFLAGS)

tags:
	ctags -R .
clean:
	rm -f $(HPDEMO_LIB_TARGET) tags

prefix=/usr/local
LIBDIR=$(prefix)/lib
BINDIR=$(prefix)/bin
install-lib: $(HPDEMO_LIB_TARGET)
	mkdir -p "$(DESTDIR)$(LIBDIR)"
	install -p $^ "$(DESTDIR)$(LIBDIR)"
install: install-lib

.PHONY: all tags clean install install-lib
# vi: set ts=8 noet :
