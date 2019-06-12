#include "SpeadTx.h"

SpeadTx::SpeadTx(){

}

void SpeadTx::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    if(inPacket->isEOS()){
        std::cout <<"SpeadTx Class: End of stream" << std::endl;
    }else{
        std::cout <<"Ready To Tx "<< std::endl;
    }
    pipelineCounts.Spead2TxStage++;
}