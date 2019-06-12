/* Copyright 2015 SKA South Africa
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <utility>
#include <boost/asio.hpp>
#include <spead2/common_endian.h>
#include <spead2/common_thread_pool.h>
#include <spead2/common_defines.h>
#include <spead2/common_flavour.h>
#include <spead2/send_heap.h>
#include <spead2/send_udp.h>
#include <spead2/send_stream.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <iomanip>
#include <atomic>

#define N_ANTS 64
#define N_CHANNELS 4096
#define N_X_ENGINES_PER_ANT 4
#define N_CHANNELS_PER_X_ENGINE N_CHANNELS/N_ANTS/N_X_ENGINES_PER_ANT
#define N_POL 2
#define TIME_SAMPLES_PER_PACKET 256
#define NUM_PACKETS 10000000

enum SampleDataFormat{one_ant_test,two_ant_test,ramp,all_zero};
SampleDataFormat sample_data_format = two_ant_test; 

//one_ant_test specifications
#define ANTENNA 3

//two_ant_test specific variables
#define ANTENNA1 16
#define ANTENNA2 58  

#define REPORTING_SPACE 20000

std::atomic<int> numSent;

using boost::asio::ip::udp;
std::mutex m;
auto start = std::chrono::high_resolution_clock::now();
int main()
{
    spead2::thread_pool tp;
    udp::resolver resolver(tp.get_io_service());
    udp::resolver::query query("127.0.0.1", "8888");
    auto it = resolver.resolve(query);
    spead2::send::udp_stream stream(tp.get_io_service(), *it, spead2::send::stream_config(9000, 0,64,64),100000);
    spead2::flavour f(spead2::maximum_version, 64, 48, spead2::BUG_COMPAT_PYSPEAD_0_5_2);
   
    //Create Descriptors
    spead2::send::heap h_desc(f);
    spead2::descriptor timestamp_desc;
    timestamp_desc.id = 0x1600;
    timestamp_desc.name = "timestamp";
    timestamp_desc.description = "A number to be scaled by an appropriate scale factor, provided as a KATCP sensor, to get the number of Unix seconds since epoch of the firsttime sample used to generate data in the current SPEAD heap. Consult CAM ICD [10] for the appropriate sensor.";
    timestamp_desc.format.emplace_back('u',64);
    spead2::descriptor feng_id_desc;
    feng_id_desc.id = 0x4101;
    feng_id_desc.name = "feng_id";
    feng_id_desc.description = "Uniquely identifies the F-engine source for the data. A sensor can be consulted to determine the mapping of F-engine to antenna input.";
    feng_id_desc.format.emplace_back('u',64);
    spead2::descriptor frequency_desc;
    frequency_desc.id = 0x4103;
    frequency_desc.name = "frequency";
    frequency_desc.description = "'Identifies the first channel in the band of frequencies in the SPEAD heap. Can be used to reconstruct the full spectrum.";
    frequency_desc.format.emplace_back('u',64);
    spead2::descriptor feng_raw_desc;
    feng_raw_desc.id = 0x4300;
    feng_raw_desc.name = "feng_raw";
    feng_raw_desc.description = "Raw channelised F-Engine Data";
    feng_raw_desc.numpy_header = "{'shape': (16, 256, 2 , 2), 'fortran_order': False, 'descr': '>i1'}";

    h_desc.add_descriptor(timestamp_desc);
    h_desc.add_descriptor(feng_id_desc);
    h_desc.add_descriptor(frequency_desc);
    h_desc.add_descriptor(feng_raw_desc);
    
    stream.async_send_heap(h_desc, [] (const boost::system::error_code &ec, spead2::item_pointer_t bytes_transferred)
    {
        if (ec)
            std::cerr << ec.message() << '\n';
        else
            std::cout << "Sent " << bytes_transferred << " bytes in descriptor heap\n";
    });

    //Send data
    std::int64_t timestamp[N_ANTS];
    std::int64_t feng_id[N_ANTS];
    std::int64_t frequency[N_ANTS];
    std::int8_t feng_raw[N_ANTS][N_CHANNELS_PER_X_ENGINE*TIME_SAMPLES_PER_PACKET*N_POL*2] = {};
    spead2::send::heap h[N_ANTS];

    for (size_t j = 0; j < N_ANTS; j++)
    {
        switch(sample_data_format){
            case one_ant_test:
            {
                for(int i = 0; i<16*256*2*2;i++){
                    if(j == ANTENNA){
                        if(i%2==0){
                            feng_raw[j][i] = (int8_t)1;
                        }else{
                            feng_raw[j][i] = (int8_t)2;
                        }    
                    }else{
                        feng_raw[j][i] = (int8_t)0;
                    }   
                }
            }
            break;
            case two_ant_test:
            {
                for(int i = 0; i<16*256*2*2;i++){
                    if(j == ANTENNA1){
                        if((i+0)%2==0 && (i+0)%4 == 0){
                            feng_raw[j][i] = (int8_t)1;
                        }else if((i+1)%2==0 && (i+1)%4 == 0){
                            feng_raw[j][i] = (int8_t)2;
                        }else if((i+0)%2==0 && (i+0)%4 != 0){
                            feng_raw[j][i] = (int8_t)3;
                        }else{
                            feng_raw[j][i] = (int8_t)4;
                        }        
                    }else if (j == ANTENNA2)
                    {
                        if((i+0)%2==0 && (i+0)%4 == 0){
                            feng_raw[j][i] = (int8_t)5;
                        }else if((i+1)%2==0 && (i+1)%4 == 0){
                            feng_raw[j][i] = (int8_t)6;
                        }else if((i+0)%2==0 && (i+0)%4 != 0){
                            feng_raw[j][i] = (int8_t)7;
                        }else{
                            feng_raw[j][i] = (int8_t)8;
                        }  
                    }else{
                        feng_raw[j][i] = (int8_t)0;
                    }   
                }
            }
            break;
            case all_zero:
            {
                for(int i = 0; i<16*256*2*2;i++){
                    feng_raw[j][i] = (int8_t) (0xff & 0);
                }
            }
            break;
            case ramp:
            default:
            {
                for(int i = 0; i<16*256*2*2;i++){
                    feng_raw[j][i] = (int8_t) (0xff & i);
                }
            }
            break;
        }
    }
    

    for (size_t k = 1; k < NUM_PACKETS; k++)
    {
        /* code */ 
        numSent=0; 
        for(int j = 0; j<N_ANTS; j++){ 
            h[j] = spead2::send::heap(f);

            timestamp[j] = (2111+k)*256*2*N_CHANNELS;
    //        std::cout << "Timestamp" << timestamp[j] << std::endl;
            feng_id[j] = j;
            frequency[j] = 2048;
                
            //std::cout << sizeof(feng_raw[j]) << std::endl;
            h[j].add_item(0x1600, timestamp[j]);
            h[j].add_item(0x4101, feng_id[j]);
            h[j].add_item(0x4103, frequency[j]);
            h[j].add_item(0x4300, &feng_raw[j],sizeof(feng_raw[j]),true); 
            
            //m.lock();

            stream.async_send_heap(h[j], [j,k] (const boost::system::error_code &ec, spead2::item_pointer_t bytes_transferred)
            {
                if (ec)
                    std::cerr << ec.message() << '\n';
                else
                {
                    numSent++;
                    //if(k%REPORTING_SPACE==0 && j == 0){
                    //    std::clock_t current = std::clock();
                    //    double duration = (current  - start ) / (double) CLOCKS_PER_SEC;
                     //   start=current;
                     //   std::cout<< k << duration << " " << std::endl;
                    //}
                    //std::cout << "Sent " << bytes_transferred << " bytes in heap of f_eng:" << j <<" Packet: "<<k<< std::endl;     
                }  
            });
            if(k%REPORTING_SPACE==0 && j == 0){
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> diff = now-start;
                start=now;
                double data_rate = (double)REPORTING_SPACE*N_ANTS*N_CHANNELS_PER_X_ENGINE*TIME_SAMPLES_PER_PACKET*N_POL*2*8/diff.count()/1000/1000/1000;
                std::cout <<std::fixed<<std::setprecision(2)<<k<< " Sets Sent. Data Rate: "<< data_rate << " Gbps. 64*"<<REPORTING_SPACE <<" packets over "<< diff.count()<< "s" <<std::endl;
                //std::cout<< k << " sets of 64 packets sent, time per packet : "<< diff.count()/64/REPORTING_SPACE*1000*1000 << " us. Time per set of 64: "<< diff.count()/REPORTING_SPACE*1000 << "ms. Time per block: " <<diff.count()<< std::endl;
            }
  
        }
        while(true){
            if(numSent>=N_ANTS){
                std::chrono::microseconds timespan(150); // or whatever
                std::this_thread::sleep_for(timespan); 
                //std::cout << numSent << std::endl;
                numSent==0;
                break;
            }
        }
    }

    spead2::send::heap end(f);
    end.add_end();
    stream.async_send_heap(end, [] (const boost::system::error_code &ec, spead2::item_pointer_t bytes_transferred) {});
    stream.flush();

    return 0;
}
