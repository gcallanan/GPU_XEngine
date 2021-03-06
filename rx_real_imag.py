## \file rx_real_imag.py
 #
 # \brief A file for plotting the real and imaginary values of the baselines produced by the GPU X-Engine. 
 #
 # \author Gareth Callanan
 #
 # This file can subscribe to the output of a GPU X-Engine and then plots the real and imaginary components for each interpolarisation product for a single antenna pair
 

from __future__ import print_function, division
import spead2
import spead2.recv
import spead2.send
import logging
import numpy as np
import matplotlib.pyplot as plt
import argparse

logging.basicConfig(level=logging.INFO)

NUM_ACCUMULATIONS = 408
NUM_CHANNELS_PER_XENGINE=16
NUM_BASELINES=2112

print("Receive Output from GPU Correlator")

parser = argparse.ArgumentParser(
    description='Monitor and plot incoming X-Engine stream data.',
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument(
    '-p', '--rx_port', dest='port', action='store', default=9888, type=int,
    help='bind to this port to receive X-Engine stream')
parser.add_argument(
    '-i', '--ip_address', dest='ip_address', action='store', default="127.0.0.1",
    help='recieve data from this IP address')
parser.add_argument(
    '-x', '--ant1', dest='ant1', action='store', default=17, type=int,
    help='First antenna to observe')
parser.add_argument(
    '-y', '--ant2', dest='ant2', action='store', default=17, type=int,
    help='Second Antenna to observe')
args = parser.parse_args()

ant1 = args.ant1
ant2 = args.ant2

def getBaselineOffset(ant0,ant1):
    if(ant0>ant1):
        raise("Condition a0<=a1 does not hold")
    quadrant = 2*(ant0&1) + (ant1&1)
    quadrant_index = int(ant1/2)*(int(ant1/2) + 1)/2 + int(ant0/2)
    #print(quadrant_index)
    return int(quadrant*(528) + quadrant_index)

def getBaseline(data_arr, i, j, polarisationProduct,poll):
    array = [0]*16
    #print("Input Packet Sum: ", np.sum(data_arr))
    #print(data_arr.shape[0])
    #print(data_arr[0])
    #print(np.nonzero(data_arr[0]))
    for k in range(0,NUM_CHANNELS_PER_XENGINE):
        baseline_index = getBaselineOffset(i,j)
        array[k] = data_arr[k][baseline_index][polarisationProduct][poll]/256/NUM_ACCUMULATIONS
#        print(k,baseline_index,i,j)
#        print(data_arr[k][baseline_index][0][0]/256/1600)
    #print(array)
    return array



items = []

#Transmit a single descriptor packet

descriptor_sent = False

thread_pool_send = spead2.ThreadPool()
stream_send = spead2.send.UdpStream(
thread_pool_send, args.ip_address, args.port, spead2.send.StreamConfig(rate=1e7))
del thread_pool_send

shape = (16, 2112, 4, 2)
ig_send = spead2.send.ItemGroup(flavour=spead2.Flavour(4, 64, 48))
item = ig_send.add_item(0x1800, 'xeng_raw', 'Raw X_ENgine Samples', shape=shape, dtype=np.int32)
item = ig_send.add_item(0x1600,'timestamp','timestamp description', shape=[],format=[('u', 48)])
item = ig_send.add_item(0x4103,'frequency','Identifies the first channel in the band of frequency channels in the SPEAD heap', shape=[],format=[('u', 48)])
#item.value = np.zeros(shape, np.int32)

#Receive 
thread_pool_recv = spead2.ThreadPool()
stream_recv = spead2.recv.Stream(thread_pool_recv)
del thread_pool_recv
pool_recv = spead2.MemoryPool(16384, 26214400, 12, 8)
stream_recv.set_memory_allocator(pool_recv)
stream_recv.add_udp_reader(9888)

fig = plt.figure()
fig.suptitle('Ant 1: {}, Ant 2: {}, Packets Received {},Timetamp: {}'.format(ant1,ant2,0,0))
plt.ion()
ax1 = fig.add_subplot(2,2,1)
ax1.set_title('<Pol 1, Pol1>')
plt.grid()
ax2 = fig.add_subplot(2,2,2)
ax2.set_title('<Pol 1, Pol2>')
plt.grid()
ax3 = fig.add_subplot(2,2,3)
ax3.set_title('<Pol 2, Pol1>')
plt.grid()
ax4 = fig.add_subplot(2,2,4)
ax4.set_title('<Pol 2, Pol2>')
plt.grid()
den = 1
line11, = ax1.plot(range(0,16), range(-128//den,128//den,16//den), 'b-x')
line11.set_label('R')
line12, = ax1.plot(range(0,16), range(-128//den,128//den,16//den), 'r-x')
line12.set_label('I')
line21, = ax2.plot(range(0,16), range(-128//den,128//den,16//den), 'b-x')
line21.set_label('R')
line22, = ax2.plot(range(0,16), range(-128//den,128//den,16//den), 'r-x')
line22.set_label('I')
line31, = ax3.plot(range(0,16), range(-128//den,128//den,16//den), 'b-x')
line31.set_label('R')
line32, = ax3.plot(range(0,16), range(-128//den,128//den,16//den), 'r-x')
line32.set_label('I')
line41, = ax4.plot(range(0,16), range(-128//den,128//den,16//den), 'b-x')
line41.set_label('R')
line42, = ax4.plot(range(0,16), range(-128//den,128//den,16//den), 'r-x')
line42.set_label('I')

ax1.legend()
ax1.set_xlabel('Frequency Channel')
#ax1.set_ylabel('')
ax2.legend()
ax2.set_xlabel('Frequency Channel')
ax3.legend()
ax3.set_xlabel('Frequency Channel')
ax4.legend()
ax4.set_xlabel('Frequency Channel')
fig.show()
#fig.set_visible(False)

print("Ready")
ig = spead2.ItemGroup()
num_heaps = 0
for heap in stream_recv:
    #print("Received")
    if(descriptor_sent==False):
        stream_send.send_heap(ig_send.get_heap())
        descriptor_sent=True

    #print("Got heap", heap.cnt)
    items = ig.update(heap)
    for item in items.values():
        if(item.id==0x1800):#print(heap.cnt, item.name, hex(item.value))
            baseline = getBaseline(item.value,ant1,ant2,0,0)
            print("Heap: ",heap.cnt," Samples:",np.around(baseline,decimals=2))
            line11.set_ydata(baseline)
            baseline = getBaseline(item.value,ant1,ant2,0,1)
            line12.set_ydata(baseline)
            baseline = getBaseline(item.value,ant1,ant2,1,0)
            line21.set_ydata(baseline)
            baseline = getBaseline(item.value,ant1,ant2,1,1)
            line22.set_ydata(baseline)
            baseline = getBaseline(item.value,ant1,ant2,2,0)
            line31.set_ydata(baseline)
            baseline = getBaseline(item.value,ant1,ant2,2,1)
            line32.set_ydata(baseline)
            baseline = getBaseline(item.value,ant1,ant2,3,0)
            line41.set_ydata(baseline)
            baseline = getBaseline(item.value,ant1,ant2,3,1)
            line42.set_ydata(baseline)
            fig.canvas.draw()
        if(item.id==0x1600):
            fig.suptitle('Ant 1: {}, Ant 2: {}, Heaps: {},Timetamp: {}'.format(ant1,ant2,num_heaps,hex(item.value)))
    num_heaps += 1
    plt.pause(0.01)

stream_recv.stop()
print("Received", num_heaps, "heaps")
