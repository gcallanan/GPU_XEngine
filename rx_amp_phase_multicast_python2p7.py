#!/usr/bin/env python

# Copyright 2015 SKA South Africa
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from __future__ import print_function, division
import spead2
import spead2.recv
import spead2.send
import logging
import numpy as np
import matplotlib.pyplot as plt
import argparse
import math

logging.basicConfig(level=logging.INFO)

NUM_ACCUMULATIONS = 408
NUM_CHANNELS_PER_XENGINE=16
NUM_BASELINES=2112

print("Receive Output from SKARAB Correlator")

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
parser.add_argument(
    '-1', '--p1', dest='p1', action='store', default=0, type=int,
    help='Second Antenna to observe')
parser.add_argument(
    '-2', '--p2', dest='p2', action='store', default=0, type=int,
    help='Second Antenna to observe')
parser.add_argument(
    '-l','--log10', action='store_true', default=False,
    help='Display the power as a logarithmic value')
parser.add_argument(
    '-s','--single_frame', action='store_true', default=False,
    help='Only display a single frame before halting the program')
args = parser.parse_args()

ant1 = args.ant1
ant2 = args.ant2
useLog10 = args.log10

p1 = args.p1
p2 = args.p2

def getBaselineOffset(ant0,ant1):
    if(ant0>ant1):
        raise("Condition a0<=a1 does not hold")
    quadrant = 2*(ant0&1) + (ant1&1)
    quadrant_index = int(ant1/2)*(int(ant1/2) + 1)/2 + int(ant0/2)
    #print(quadrant_index)
    return int(quadrant*(528) + quadrant_index)

def getBaseline(data_arr, i, j, polarisationProduct,poll):
    array = [0]*16
    for k in range(0,NUM_CHANNELS_PER_XENGINE):
        if(j==50):
            baseline_index = 7637#417#getBaselineOffset(i,j)
        elif(j==17):
            baseline_index = 417
        else:
            raise Exception("Only ant2 of 17 or 50 supported at this stage")
        array[k] = 1.0*data_arr[k][baseline_index][poll]#/256/NUM_ACCUMULATIONS
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
item = ig_send.add_item(0x1800, 'xeng_raw', 'Raw X_Engine Samples', shape=shape, dtype=np.int32)
item = ig_send.add_item(0x1600,'timestamp','timestamp description', shape=[],format=[('u', 48)])
item = ig_send.add_item(0x4103,'frequency','Identifies the first channel in the band of frequency channels in the SPEAD heap', shape=[],format=[('u', 48)])
item.value = np.zeros(shape, np.int32)

#Receive 
thread_pool_recv = spead2.ThreadPool()
stream_recv = spead2.recv.Stream(thread_pool_recv)
del thread_pool_recv
pool_recv = spead2.MemoryPool(16384, 26214400, 12, 8)
stream_recv.set_memory_allocator(pool_recv)
stream_recv.add_udp_reader(multicast_group=args.ip_address,port=7148,interface_address='10.100.3.65')

fig = plt.figure()
fig.suptitle('SKARAB X-Engine Output\n Ant 1: {}, Ant 2: {}, Packets Received {},Timetamp: {}'.format(ant1,ant2,0,0),fontsize=18)
plt.ion()
ax1 = fig.add_subplot(2,1,1)
#ax1.set_title('Ant 1 Poll {}, Poll 2 {}'.format(p1,p2))
plt.grid()
ax2 = fig.add_subplot(2,1,2)
ant1 = args.ant1
ant2 = args.ant2
plt.grid()
den = 1
if(useLog10):
    line11, = ax1.plot(range(0,16), range(-60,4//den,4//den), 'b-x')
else:
    line11, = ax1.plot(range(0,16), range(0,2**31//den,2**27//den), 'b-x')
line21, = ax2.plot(range(0,16), np.linspace(-math.pi,math.pi,num=16), 'b-x')

if (useLog10):
    ax1.set_ylabel('Power Level(dBFS)',fontsize=14)
else:
    ax1.set_ylabel('Power Level(linear)',fontsize=14)
ax1.set_xlabel('Frequency Channel',fontsize=14)
ax1.set_title('Signal Power',fontsize=15)

#ax2.legend()
ax2.set_xlabel('Frequency Channel',fontsize=14)
ax2.set_ylabel('Phase(rad)',fontsize=14)
ax2.set_title('Signal Phase',fontsize=15)
fig.show()
fig.canvas.draw()

print("Ready")
ig = spead2.ItemGroup()
num_heaps = 0
firstPlot=False
for heap in stream_recv:
    print("Received")
    descr = (heap.get_descriptors())
    for temp in descr:
        print(temp.id,temp.name,temp.shape)
    if(descriptor_sent==False):
        #stream_send.send_heap(ig_send.get_heap())
        descriptor_sent=True

    #print("Got heap", heap.cnt)
    items = ig.update(heap)
    hasCorrectValue = False
    print(0x1600,0x1800,0x4103)
    for item in items.values():
        print(item.id, item.shape)
        if(item.id==0x1800):#print(heap.cnt, item.name, hex(item.value))
            hasCorrectValue = True
            #baseline = getBaseline(item.value,ant1,ant2,0,0)
            #print(baseline)
            productIndex = p2 * 2 + p1; 
            realValues = np.array(getBaseline(item.value,ant1,ant2,productIndex,0))
            imagValues = np.array(getBaseline(item.value,ant1,ant2,productIndex,1))
            print(realValues)
            print(imagValues)
            pwr = np.abs(np.sqrt(np.square(realValues) + np.square(imagValues)))
            print((10*np.log10(np.divide(pwr,(2**31)))))
            print()
            phase = np.arctan2(imagValues,realValues)
            if(useLog10):
                line11.set_ydata(10*np.log10(np.divide(pwr,(2**31))))
            else:
                line11.set_ydata(pwr)
            line21.set_ydata(phase)
            fig.canvas.draw()
            firstPlot=True
        if(item.id==0x1600):
            fig.suptitle('SKARAB X-Engine Output\nAnt 1: {}, Ant 2: {}, Heaps: {},Timetamp: {}'.format(ant1,ant2,num_heaps,hex(item.value)))
    if(~hasCorrectValue): print("item id 0x1800 missing")
    num_heaps += 1
    plt.pause(0.01)
    if(args.single_frame and firstPlot): 
        #stream_recv.
        break

stream_recv.stop()
print("Received", num_heaps, "heaps")
