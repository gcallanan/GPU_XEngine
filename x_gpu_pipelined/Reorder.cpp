#include "Reorder.h"

Reorder::Reorder(){

}

void Reorder::operator()(boost::shared_ptr<StreamObject> inPacket, multi_node::output_ports_type &op){
    //std::get<0>(op).try_put(tbb::flow:continue_msg());//(std::make_shared<StreamObject>());
}