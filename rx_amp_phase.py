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
parser.add_argument(
    '-1', '--p1', dest='p1', action='store', default=0, type=int,
    help='Second Antenna to observe')
parser.add_argument(
    '-2', '--p2', dest='p2', action='store', default=0, type=int,
    help='Second Antenna to observe')
parser.add_argument(
    '-l','--log10', action='store_true', default=False,
    help='Display the power as a logarithmic value')
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
        baseline_index = getBaselineOffset(i,j)
        array[k] = data_arr[k][baseline_index][polarisationProduct][poll]#/256/NUM_ACCUMULATIONS
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
ax1 = fig.add_subplot(2,1,1)
ax1.set_title('Ant 1 Poll {}, Poll 2 {}'.format(p1,p2))
plt.grid()
ax2 = fig.add_subplot(2,1,2)
ant1 = args.ant1
ant2 = args.ant2
plt.grid()
den = 1
if(useLog10):
    line11, = ax1.plot(range(0,16), range(-96,12//den,7//den), 'b-x')
else:
    line11, = ax1.plot(range(0,16), range(0,2**31//den,2**27//den), 'b-x')
line21, = ax2.plot(range(0,16), np.linspace(-math.pi,math.pi,num=16), 'b-x')

ax1.legend()
if ~useLog10:
    ax1.set_xlabel('Magnitude - Linear')
else:
    ax1.set_xlabel('Magnitude - dB')
#ax1.set_ylabel('')
ax2.legend()
ax2.set_xlabel('Phase')
fig.show()

print("Ready")
ig = spead2.ItemGroup()
num_heaps = 0
for heap in stream_recv:
    print("Received")
    if(descriptor_sent==False):
        stream_send.send_heap(ig_send.get_heap())
        descriptor_sent=True

    print("Got heap", heap.cnt)
    items = ig.update(heap)
    for item in items.values():
        if(item.id==0x1800):#print(heap.cnt, item.name, hex(item.value))
            baseline = getBaseline(item.value,ant1,ant2,0,0)
            #print(baseline)
            productIndex = p2 * 2 + p1; 
            realValues = np.abs(getBaseline(item.value,ant1,ant2,productIndex,0))
            imagValues = np.abs(getBaseline(item.value,ant1,ant2,productIndex,1))
            print(realValues)
            #print(imagValues)
            pwr = np.abs(np.square(realValues) + np.square(imagValues))
            #print((10*np.log10(np.divide(pwr,(2**31)))))
            print()
            phase = np.arctan2(imagValues,realValues)
            if(useLog10):
                line11.set_ydata(10*np.log10(np.divide(pwr,(2**31))))
            else:
                line11.set_ydata(pwr)
            line21.set_ydata(phase)
            fig.canvas.draw()
        if(item.id==0x1600):
            fig.suptitle('Ant 1: {}, Ant 2: {}, Heaps: {},Timetamp: {}'.format(ant1,ant2,num_heaps,hex(item.value)))
    num_heaps += 1
    plt.pause(0.01)

stream_recv.stop()
print("Received", num_heaps, "heaps")
