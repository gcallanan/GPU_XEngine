/**
 * \file f_engine_simulator.cpp
 *
 * \brief Standalone F-Engine Simulator. 
 *
 * \author Gareth Callanan
 *
 * This file is a standalone program that can be built on any machine with SPEAD2 installed. It is designed to simulate a single F-Engine output stream with all 64 F-Engine packets in the stream
 * 
 * It can generate data at the requried output rate as long as the stream config packet size is set to 4 KiB and above.
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
#include <boost/program_options.hpp>
#include <string>
#include <deque>

#define N_ANTS 64
#define N_CHANNELS 4096
#define N_X_ENGINES_PER_ANT 4
#define N_CHANNELS_PER_X_ENGINE N_CHANNELS/N_ANTS/N_X_ENGINES_PER_ANT
#define N_POL 2
#define TIME_SAMPLES_PER_PACKET 256

#define TIMESTAMP_VARIATION 5

#define FFT_SIZE (N_CHANNELS_PER_X_ENGINE*4*N_ANTS)

enum SampleDataFormat{one_ant_test,two_ant_test,ramp,all_zero};
SampleDataFormat sample_data_format = two_ant_test; 

//one_ant_test specifications
#define ANTENNA 3

#define REPORTING_SPACE 10000

std::atomic<int> numSent;

using boost::asio::ip::udp;
std::mutex m;
auto start = std::chrono::high_resolution_clock::now();
int main(int argc, char** argv)
{
    namespace po = boost::program_options;  
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("tx_port", po::value<std::string>()->default_value("8888"), "Set transmitter port")
        ("dest_ip", po::value<std::string>()->default_value("127.0.0.1"), "Set destination ip address")
        ("drop_rate", po::value<std::int32_t>()->default_value(0), "Set packet drop rate in percent(range: 0 to 100)")
        ("rate_fudge_factor", po::value<std::int32_t>()->default_value(150), "Set delay after each batch of 64 packets.")
        ("ant1", po::value<std::int32_t>()->default_value(17), "Set 1st Antenna to add non-zero data to.(range: 0 to 63)")
        ("ant2", po::value<std::int32_t>()->default_value(17), "Set 2nd Antenna to add non-zero data to.(range: 0 to 63)")
        ("num_packets_total", po::value<std::int32_t>()->default_value(1000000), "Total number of sets of 64 packets to send.")
        ("sync_start", po::value<int64_t>()->default_value(44040089), "Set number of accumulations")
        ("accumulation_length", po::value<int32_t>()->default_value(408), "Set number of accumulations")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")){
        std::cout << desc << "\n";
        return 1;
    }

    std::string txPort = vm["tx_port"].as<std::string>();
    std::string destIp = vm["dest_ip"].as<std::string>();
    std::int32_t dropRate = vm["drop_rate"].as<std::int32_t>();
    std::int32_t accLength = vm["accumulation_length"].as<std::int32_t>();
    int syncStart = vm["sync_start"].as<int64_t>();

    if(dropRate < 0 || dropRate >100){
        std::cout <<  "ERROR: Drop rate set to "<<dropRate<<". Not in range of 0 to 100." << std::endl << "Exiting Program" << std::endl;
        return -1;
    }
    std::int32_t rateFudgeFactor = vm["rate_fudge_factor"].as<std::int32_t>();
    std::int32_t ant1 = vm["ant1"].as<std::int32_t>();
    if(ant1 < 0 || ant1 >63){
        std::cout <<  "ERROR: ant1 set to "<<ant1<<". Not in range of 0 to 63." << std::endl << "Exiting Program" << std::endl;
        return -1;
    }
    std::int32_t ant2 = vm["ant2"].as<std::int32_t>();
    if(ant2 < 0 || ant2 >63){
        std::cout <<  "ERROR: ant1 set to "<<ant2<<". Not in range of 0 to 63." << std::endl << "Exiting Program" << std::endl;
        return -1;
    }
    std::int32_t numPackets = vm["num_packets_total"].as<std::int32_t>();

    spead2::thread_pool tp(10);
    udp::resolver resolver(tp.get_io_service());
    udp::resolver::query query(destIp, txPort);
    auto it = resolver.resolve(query);
    spead2::send::udp_stream stream(tp.get_io_service(), *it, spead2::send::stream_config(9000, 0,64,64),100000);
    spead2::flavour f(spead2::maximum_version, 64, 48);
   
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

    int8_t array_temp[8] = {1,1,1,1,1,1,1,1};//{-3,-2,-1,1,2,3,4,5};
    int arrayPos = 0;
    bool regen = 1;
    int droppedPackets = 0;

   
    //Generates an array of queus to make the timestamps out of order - this whole system could be done better but the out of order transmision was only added near the end
    std::vector<std::uint64_t> timestampArrayOfQueues[N_ANTS];
    for (int k = 0; k < TIMESTAMP_VARIATION-1; k++)
    {
        //std::cout << k << std::endl;
        for (size_t j = 0; j < N_ANTS; j++)
        {
            timestampArrayOfQueues[j].push_back((syncStart*FFT_SIZE*2*TIME_SAMPLES_PER_PACKET+k));
        }
    }

    for(size_t k = 1; k < numPackets; k++)
    {
        //<<<<<<<<<<START SECTION THAT GENERATES DATA>>>>>>>>>>>
        if(regen == 1){
            //std::cout <<timestampArrayOfQueues[0][0] << " " << timestampArrayOfQueues[0][0]/256/8192%accLength << std::endl;
            regen=0;
            for(int tempI = 0; tempI < 8; tempI++){
                array_temp[tempI]=(array_temp[tempI]+1) %5;
            }
            for (size_t j = 0; j < N_ANTS; j++)
            {
                switch(sample_data_format){
                    case one_ant_test:
                    {
                        for(int i = 0; i<16*256*2*2;i++){
                            if(j == ANTENNA){
                                if(i%2==0){
                                    feng_raw[j][i] = (int8_t)array_temp[arrayPos%8+0];
                                }else{
                                    feng_raw[j][i] = (int8_t)array_temp[arrayPos%8+1];
                                }    
                            }else{
                                feng_raw[j][i] = (int8_t)0;
                            }   
                        }
                    }
                    break;
                    case two_ant_test:
                    {
                        for (size_t f = 0; f < N_CHANNELS_PER_X_ENGINE; f++)
                        {
                            for(int i = 0; i<256*2*2;i++){
                                if(j == ant1){
                                    if((i+0)%2==0 && (i+0)%4 == 0){
                                        feng_raw[j][f*256*2*2 + i] = (int8_t)array_temp[(arrayPos+0+f)%8];
                                    }else if((i+1)%2==0 && (i+1)%4 == 0){
                                        feng_raw[j][f*256*2*2+i] = (int8_t)array_temp[(arrayPos+1+f)%8];
                                    }else if((i+0)%2==0 && (i+0)%4 != 0){
                                        feng_raw[j][f*256*2*2+i] = (int8_t)array_temp[(arrayPos+2+f)%8];
                                    }else{
                                        feng_raw[j][f*256*2*2+i] = (int8_t)array_temp[(arrayPos+4+f)%8];
                                    }        
                                }else if (j == ant2)
                                {
                                    if((i+0)%2==0 && (i+0)%4 == 0){
                                        feng_raw[j][f*256*2*2+i] = (int8_t)array_temp[(arrayPos+3+f)%8];
                                    }else if((i+1)%2==0 && (i+1)%4 == 0){
                                        feng_raw[j][f*256*2*2+i] = (int8_t)array_temp[(arrayPos+5+f)%8];
                                    }else if((i+0)%2==0 && (i+0)%4 != 0){
                                        feng_raw[j][f*256*2*2+i] = (int8_t)array_temp[(arrayPos+6+f)%8];
                                    }else{
                                        feng_raw[j][f*256*2*2+i] = (int8_t)array_temp[(arrayPos+7+f)%8];
                                    }  
                                }else{
                                    feng_raw[j][i] = (int8_t)0;
                                }   
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
            //std::cout << (int)array_temp[(arrayPos+0)%8] << " " << (int)array_temp[(arrayPos+3)%8] << std::endl;
            arrayPos++;
        }
        //<<<<<<<<<<END SECTION THAT GENERATES DATA>>>>>>>>>>>

        //<<<<<<<<<<START SECTION THAT GENERATES HEAPS>>>>>>>>>>> 
        numSent=0; 
        for(int j = N_ANTS-1; j>=0; j--){ 
            h[j] = spead2::send::heap(f);

            timestampArrayOfQueues[j].push_back(((syncStart+k)*FFT_SIZE*2*TIME_SAMPLES_PER_PACKET));
            //if(k%2==0){
                timestamp[j] = timestampArrayOfQueues[j].front();
                timestampArrayOfQueues[j][0];
                timestampArrayOfQueues[j].erase(timestampArrayOfQueues[j].begin());
            //}else{
            //    const int randPos = std::rand() % TIMESTAMP_VARIATION;
            //    timestamp[j] = timestampArrayOfQueues[j].at(randPos);
            //    timestampArrayOfQueues[j].erase(timestampArrayOfQueues[j].begin()+randPos);
            //}

            //if(j == 0){
            //    std::cout << timestamp[j] << std::endl;
            //}
            //std::cout << "Timestamp" << timestamp[j] << std::endl;
            feng_id[j] = j;
            frequency[j] = 2048;
                
            //std::cout << sizeof(feng_raw[j]) << std::endl;
            h[j].add_item(0x1600, timestamp[j]);
            h[j].add_item(0x4101, feng_id[j]);
            h[j].add_item(0x4103, frequency[j]);
            h[j].add_item(0x4300, &feng_raw[j],sizeof(feng_raw[j]),true); 

            if(j == 0){
                int64_t accNum = (timestamp[j]/256/8192-syncStart)%accLength;
                if(accNum==accLength-1){
                    regen=1;
                    //std::cout << "Regen" << std::endl;
                }
                //std::cout << "Remainder: " << accNum << ", Timestamp Diff: " << timestampArrayOfQueues[j][0]-timestamp[j] << ", Ant: " << j << std::endl;
            }
            
            //m.lock();
        }
        //<<<<<<<<<<END SECTION THAT GENERATES HEAPS>>>>>>>>>>>


        //<<<<<<<<<<START SECTION THAT TRANSMITS HEAPS>>>>>>>>>>>
        for(int j = N_ANTS-1; j>=0; j--){
            if(std::rand()%100 >= dropRate){
                stream.async_send_heap(h[j], [j,k] (const boost::system::error_code &ec, spead2::item_pointer_t bytes_transferred)
                {
                    if (ec){
                        std::cerr << ec.message() << '\n';
                        std::cout << "Error Sending Data" << std::endl;
                    }else
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
            }else{
                numSent++;
                droppedPackets++;
            }
            //<<<<<<<<<<END SECTION THAT TRANSMITS HEAPS>>>>>>>>>>>

            //<<<<<<<<<<START SECTION THAT DETERMINES WHETHER TO PRINT OUT RATE AND REGEN DATA>>>>>>>>>>>
            if(k%REPORTING_SPACE==0 && j == 0){
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> diff = now-start;
                start=now;
                double data_rate = (double)REPORTING_SPACE*N_ANTS*N_CHANNELS_PER_X_ENGINE*TIME_SAMPLES_PER_PACKET*N_POL*2*8/diff.count()/1000/1000/1000;
                std::cout <<std::fixed<<std::setprecision(2)<<k<< " Sets Sent. Data Rate: "<< data_rate << " Gbps. 64*"<<REPORTING_SPACE <<" packets over "<< diff.count()<< "s. "<< "Drop Rate: " << ((double)droppedPackets)/(REPORTING_SPACE*64+0.0)*100 <<"%"<<std::endl;
                //std::cout<< k << " sets of 64 packets sent, time per packet : "<< diff.count()/64/REPORTING_SPACE*1000*1000 << " us. Time per set of 64: "<< diff.count()/REPORTING_SPACE*1000 << "ms. Time per block: " <<diff.count()<< std::endl;
                droppedPackets = 0;
            }
            //if(){ //Figure out the condition to make regen happen, check timestamp generation is deterministic
            //    regen=1;
            //}
            //<<<<<<<<<<END SECTION THAT DETERMINES WHETHER TO PRINT OUT RATE AND REGEN DATA>>>>>>>>>>>
        }
        
        while(true){
            if(numSent>=N_ANTS){
                std::chrono::microseconds timespan(rateFudgeFactor); // or whatever
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
