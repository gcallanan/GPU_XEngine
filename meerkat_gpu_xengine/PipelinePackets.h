#ifndef _PIPELINE_PACKETS_H
#define _PIPELINE_PACKETS_H

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "global_definitions.h"
#include "XGpuBufferManager.h"

/**
 * \brief A base class for packets to be passed between stages in the pipline.
 * 
 * This base class does not containe any stage specific variables. All packets passed on the pipeline will have this class as their parent
 * 
 * \author Gareth Callanan
 */
class PipelinePacket{
    public:
        /** 
         * \brief Constructor for standard Stream Object packet
         * 
         * \param eos True for End of Stream(EOS), false otherwise
         * \param timestamp_u64 Packet Timestamp
         * \param frequency Base Frequency of packet. Frequency in packet ranges from frequency to frequency+NUM_CHANNELS_PER_XENGINE
         */
        PipelinePacket(uint64_t timestamp_u64,bool eos,uint64_t frequency): timestamp_u64(timestamp_u64),eos(eos),frequency(frequency){

        }

        /** 
         * \brief Constructor for empty packet
         * 
         * Creates an empty packet with the option to set the End of Stream(EOS) flag
         * 
         * \param eos True for EOS, false otherwise
         */
        PipelinePacket(bool eos): eos(eos),timestamp_u64(0),frequency(0){

        }

        virtual uint64_t getTimestamp(){
          return timestamp_u64;
        }
        uint64_t getFrequency(){
          return frequency;
        }

        /**
         * \brief Determines if this is an End Of Stream packet
         * 
         * \return True for End of Stream, False Otherwise
         */
        bool isEOS(){
          return eos;
        }


        /**
         * \brief Comparator for < operator
         * 
         * Compares packets according to timestamps
         */
        friend bool operator<(PipelinePacket& lhs, PipelinePacket& rhs)
        {
          return lhs.getTimestamp() < rhs.getTimestamp();
        }

        /**
         * \brief Comparator for > operator
         * 
         * Compares packets according to timestamps
         */
        friend bool operator>(PipelinePacket& lhs, PipelinePacket& rhs)
        {
          return lhs.getTimestamp() > rhs.getTimestamp();
        }
    protected:
        uint64_t timestamp_u64;
        const bool eos;
        const uint64_t frequency;
};

/**
 * \brief Packet that is transmitted by the SpeadRx class
 * 
 * Contains a pointer to a single SPEAD heap. This heap has an associated F-Engine ID on top of the other general PipelinePacket parameters
 * 
 * \author Gareth Callanan
 */
class SpeadRxPacket: public PipelinePacket
{
    public:
        /** 
         * \brief Constructor for standard Stream Object packet
         * 
         * \param eos True for End of Stream(EOS), false otherwise
         * \param timestamp_u64 Packet Timestamp
         * \param frequency Base Frequency of packet. Frequency in packet ranges from frequency to frequency+NUM_CHANNELS_PER_XENGINE
         * \param fheap Pointer to the heap received from a SPEAD2 stream
         * \param payloadPtr_p Pointer to the payload of the heap. This prevents this value having to be re-calculated.
         * \param fEngineId The ID of the F-Engine that this packet is from
         */
        SpeadRxPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,uint64_t fEngineId,uint8_t *payloadPtr_p, boost::shared_ptr<spead2::recv::heap>fheap): PipelinePacket(timestamp_u64,eos,frequency),fEngineId(fEngineId),fheap(fheap),payloadPtr_p(payloadPtr_p){

        }
        uint64_t getFEngineId(){
          return fEngineId;
        }
        boost::shared_ptr<spead2::recv::heap> getHeapPtr(){
          return fheap;
        }
        uint8_t * getPayloadPtr_p(){
          return payloadPtr_p;
        }
        
    private:
        uint64_t fEngineId;
        uint8_t * payloadPtr_p;
        boost::shared_ptr<spead2::recv::heap> fheap;

};

class BufferPacket: public virtual PipelinePacket{
    public:
        BufferPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency) : PipelinePacket(timestamp_u64,eos,frequency),fEnginesPresent_u64(0),numFenginePacketsProcessed(0),heaps_v(){
            heaps_v.clear();
            heaps_v.reserve(NUM_ANTENNAS);
            data_pointers_v.clear();
            data_pointers_v.reserve(NUM_ANTENNAS);
            for (size_t i = 0; i < NUM_ANTENNAS; i++)
            {
              heaps_v.push_back(nullptr);
              data_pointers_v.push_back(nullptr);
            }
            
        }
        
        void addPacket(int antIndex,boost::shared_ptr<spead2::recv::heap> fheap,uint8_t* data_ptr){
            if(!this->isPresent(antIndex)){
              heaps_v[antIndex]=fheap;
              data_pointers_v[antIndex] = data_ptr;
              numFenginePacketsProcessed++;
              fEnginesPresent_u64 |= 1UL << antIndex;
            }else{
              std::cout << "Received a duplicate packet" << std::endl;
              throw "Received a duplicate packet";
            }
        }

        bool isPresent(int antIndex){
            return (fEnginesPresent_u64 >> antIndex) & 1U;
        }

        int numPacketsReceived(){
          return numFenginePacketsProcessed;
        }

        uint8_t * getDataPtr(int antIndex){
          return data_pointers_v[antIndex];
        }

    private:
        uint8_t numFenginePacketsProcessed;/**< Number of F-Engine packets recieved, equal to NUM_ANTENNAS if all packets are received, missing antennas should have their data zeroed*/
        uint64_t fEnginesPresent_u64;/**< The bits in this field from 0 to NUM_ANTENNAS will be set to 1 if the corresponding F-Engine packet has been recieved or 0 otherwise.. */
        std::vector<boost::shared_ptr<spead2::recv::heap>> heaps_v;
        std::vector<uint8_t*> data_pointers_v;
};

class PacketArmortiser: public virtual PipelinePacket{
    public:
      PacketArmortiser(): PipelinePacket(false){
      }
      void addPacket(boost::shared_ptr<PipelinePacket> packetIn){
        packets.push_back(packetIn);
      }
      boost::shared_ptr<PipelinePacket> removePacket(){
        boost::shared_ptr<PipelinePacket> outPacket = packets.front();
        packets.pop_front();
        return outPacket;
      }
      int getArmortiserSize(){
        return packets.size();
      }
    private:
      std::deque<boost::shared_ptr<PipelinePacket>> packets;

};

class TransposePacket: public virtual PipelinePacket{
    public:
        TransposePacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,boost::shared_ptr<XGpuBufferManager> xGpuBufferManager,boost::shared_ptr<BufferPacket> inputData_p): PipelinePacket(timestamp_u64,eos,frequency),xGpuBufferManager(xGpuBufferManager),packetData(xGpuBufferManager->allocateMemory_CpuToGpu()),inputData_p(inputData_p){
        
        }
        uint8_t * getDataPointer(){
            return packetData.data_ptr; 
        }
        int getBufferOffset(){
            return packetData.offset;
        }
        boost::shared_ptr<BufferPacket> getInputData_ptr(){
          return inputData_p;
        }
        void clearInputData(){
          inputData_p = nullptr;
        }
        ~TransposePacket(){
          xGpuBufferManager->freeMemory_CpuToGpu(packetData.offset);
          //std::cout<<"Deleted"<<std::endl;
        }
    private:
        XGpuInputBufferPacket packetData; 
        boost::shared_ptr<XGpuBufferManager> xGpuBufferManager;
        boost::shared_ptr<BufferPacket> inputData_p;

};

class GPUWrapperPacket: public virtual PipelinePacket{
    public:
        GPUWrapperPacket(uint64_t timestamp_u64,bool eos,uint64_t frequency,boost::shared_ptr<XGpuBufferManager> xGpuBufferManager): PipelinePacket(timestamp_u64,eos,frequency),xGpuBufferManager(xGpuBufferManager), packetData(xGpuBufferManager->allocateMemory_GpuToCpu()){
        
        }
        uint8_t * getDataPointer(){
            return packetData.data_ptr; 
        }
        int getBufferOffset(){
            return packetData.offset;
        }
        ~GPUWrapperPacket(){
          xGpuBufferManager->freeMemory_GpuToCpu(packetData.offset);
        }
        
    private:
        XGpuOutputBufferPacket packetData; 
        boost::shared_ptr<XGpuBufferManager> xGpuBufferManager;

};

typedef tbb::flow::multifunction_node<boost::shared_ptr<PipelinePacket>, tbb::flow::tuple<boost::shared_ptr<PipelinePacket> > > multi_node;

struct PipelinePacketPointerCompare
{
    bool operator()(const boost::shared_ptr<PipelinePacket>& lhs, const boost::shared_ptr<PipelinePacket>& rhs)
    {
        return *lhs > *rhs;
    }
};

#endif