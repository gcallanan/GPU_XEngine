#include "SpeadTx.h"

std::mutex accessLock;

SpeadTx::SpeadTx(std::string txPort){
    tp = boost::make_shared<spead2::thread_pool>();
    resolver = boost::make_shared<boost::asio::ip::udp::resolver>(tp->get_io_service());
    query = boost::make_shared<boost::asio::ip::udp::resolver::query>("127.0.0.1", txPort);
    auto it = resolver->resolve(*query);
    stream = boost::make_shared<spead2::send::udp_stream>(tp->get_io_service(), *it, spead2::send::stream_config(9000, 0));
    f = boost::make_shared<spead2::flavour>(spead2::maximum_version, 64, 48);
}


void SpeadTx::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    if(inPacket->isEOS()){
        std::cout <<"SpeadTx Class: End of stream" << std::endl;
    }else{
        boost::shared_ptr<GPUWrapperPacket> inPacket_cast = boost::dynamic_pointer_cast<GPUWrapperPacket>(inPacket);
        spead2::send::heap h(*f);
        std::int32_t xengRaw_p[2*4*NUM_BASELINES*NUM_CHANNELS_PER_XENGINE]={};
        int baselineIndex = 0;
        for (int k = 0; k < NUM_CHANNELS_PER_XENGINE; k++)
        {
            for (size_t i = 0; i < NUM_BASELINES; i++)
            {
                BaselineProducts_out* base = (BaselineProducts_out*)inPacket_cast->getDataPointer();
                BaselineProducts_out baselineProductReal_p = base[k*NUM_BASELINES+i];
                BaselineProducts_out baselineProductImag_p = base[k*NUM_BASELINES+i + NUM_BASELINES*NUM_CHANNELS_PER_XENGINE];
                xengRaw_p[baselineIndex] = baselineProductReal_p.product0;
                xengRaw_p[baselineIndex+1] = baselineProductImag_p.product0;
                xengRaw_p[baselineIndex+2] = baselineProductReal_p.product1;
                xengRaw_p[baselineIndex+3] = baselineProductImag_p.product1;
                xengRaw_p[baselineIndex+4] = baselineProductReal_p.product2;
                xengRaw_p[baselineIndex+5] = baselineProductImag_p.product2;
                xengRaw_p[baselineIndex+6] = baselineProductReal_p.product3;
                xengRaw_p[baselineIndex+7] = baselineProductImag_p.product3;
                baselineIndex=baselineIndex+8;
            }
        }
        h.add_item(0x4103,inPacket_cast->getFrequency());
        h.add_item(0x1600,inPacket_cast->getTimestamp());
        h.add_item(0x1800,&xengRaw_p,sizeof(xengRaw_p), true);
        accessLock.lock();
        stream->async_send_heap(h, [] (const boost::system::error_code &ec, spead2::item_pointer_t bytes_transferred)
        {
            accessLock.unlock();
            if (ec){
                std::cerr << "Transmit Error after " << speadTxSuccessCount << " succesful transmits. Message: "<< ec.message() << '\n';
                speadTxSuccessCount = 0;
            }else{
                speadTxSuccessCount++;
            }  
        });
        accessLock.lock();
        accessLock.unlock();
    }
    pipelineCounts.Spead2TxStage++;
}