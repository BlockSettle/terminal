////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2018, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_CLIENTCLASSES
#define _H_CLIENTCLASSES

#include <exception>
#include <string>
#include <functional>

#include "BinaryData.h"
#include "SocketObject.h"
#include "BDVCodec.h"
#include "nodeRPC.h"

namespace AsyncClient
{
   class BlockDataViewer;
};
   
///////////////////////////////////////////////////////////////////////////////
struct NoArmoryDBExcept : public std::runtime_error
{
   NoArmoryDBExcept(void) : runtime_error("")
   {}
};

struct BDVAlreadyRegistered : public std::runtime_error
{
   BDVAlreadyRegistered(void) : std::runtime_error("")
   {}
};

///////////////////////////////////////////////////////////////////////////////
namespace ClientClasses
{
   void initLibrary(void);

   ///////////////////////////////////////////////////////////////////////////////
   struct FeeEstimateStruct
   {
      std::string error_;
      float val_ = 0;
      bool isSmart_ = false;

      FeeEstimateStruct(float val, bool isSmart, const std::string& error) :
         error_(error), val_(val), isSmart_(isSmart)
      {}

      FeeEstimateStruct(void) 
      {}
   };

   ///////////////////////////////////////////////////////////////////////////////
   class BlockHeader
   {
      friend class Blockchain;
      friend class testBlockHeader;
      friend class BlockData;

   private:

      void unserialize(uint8_t const * ptr, uint32_t size);
      void unserialize(BinaryDataRef const & str)
      {
         unserialize(str.getPtr(), str.getSize());
      }

   public:

      BlockHeader(void) {}
      BlockHeader(const BinaryData&, unsigned);

      uint32_t           getVersion(void) const { return READ_UINT32_LE(getPtr()); }
      BinaryData const & getThisHash(void) const { return thisHash_; }
      BinaryData         getPrevHash(void) const { return BinaryData(getPtr() + 4, 32); }
      BinaryData         getMerkleRoot(void) const { return BinaryData(getPtr() + 36, 32); }
      BinaryData         getDiffBits(void) const { return BinaryData(getPtr() + 72, 4); }
      uint32_t           getTimestamp(void) const { return READ_UINT32_LE(getPtr() + 68); }
      uint32_t           getNonce(void) const { return READ_UINT32_LE(getPtr() + 76); }
      uint32_t           getBlockHeight(void) const { return blockHeight_; }

      //////////////////////////////////////////////////////////////////////////
      BinaryDataRef  getThisHashRef(void) const { return thisHash_.getRef(); }
      BinaryDataRef  getPrevHashRef(void) const { return BinaryDataRef(getPtr() + 4, 32); }
      BinaryDataRef  getMerkleRootRef(void) const { return BinaryDataRef(getPtr() + 36, 32); }
      BinaryDataRef  getDiffBitsRef(void) const { return BinaryDataRef(getPtr() + 72, 4); }

      //////////////////////////////////////////////////////////////////////////
      uint8_t const * getPtr(void) const {
         if (!isInitialized_)
            throw std::runtime_error("uninitialized BlockHeader");
         return dataCopy_.getPtr();
      }
      size_t        getSize(void) const {
         if (!isInitialized_)
            throw std::runtime_error("uninitialized BlockHeader");
         return dataCopy_.getSize();
      }
      bool            isInitialized(void) const { return isInitialized_; }

      void clearDataCopy() { dataCopy_.resize(0); }

   private:
      BinaryData     dataCopy_;
      bool           isInitialized_ = false;
      // Specific to the DB storage
      uint32_t       blockHeight_ = UINT32_MAX;

      // Derived properties - we expect these to be set after construct/copy
      BinaryData     thisHash_;
      double         difficultyDbl_ = 0.0;
   };

   ////////////////////////////////////////////////////////////////////////////
   class LedgerEntry
   {
   private:
      std::shared_ptr<::google::protobuf::Message> msgPtr_;
      const ::Codec_LedgerEntry::LedgerEntry* ptr_ = nullptr;

   public:
      LedgerEntry(BinaryDataRef bdr);
      LedgerEntry(std::shared_ptr<::Codec_LedgerEntry::LedgerEntry>);
      LedgerEntry(std::shared_ptr<::Codec_LedgerEntry::ManyLedgerEntry>,
         unsigned);
      LedgerEntry(std::shared_ptr<::Codec_BDVCommand::BDVCallback>,
         unsigned, unsigned);

      const std::string&  getID(void) const;
      int64_t             getValue(void) const;
      uint32_t            getBlockNum(void) const;
      BinaryDataRef       getTxHash(void) const;
      uint32_t            getIndex(void) const;
      uint32_t            getTxTime(void) const;
      bool                isCoinbase(void) const;
      bool                isSentToSelf(void) const;
      bool                isChangeBack(void) const;
      bool                isOptInRBF(void) const;
      bool                isChainedZC(void) const;
      bool                isWitness(void) const;

      std::vector<BinaryDataRef> getScrAddrList(void) const;

      bool operator==(const LedgerEntry& rhs);
   };

   ////////////////////////////////////////////////////////////////////////////
   class NodeChainState
   {
   private:
      std::shared_ptr<::google::protobuf::Message> msgPtr_;
      const ::Codec_NodeStatus::NodeChainState* ptr_;

   public:
      NodeChainState(std::shared_ptr<::Codec_NodeStatus::NodeStatus>);

      unsigned getTopBlock(void) const;
      ChainStatus state(void) const;
      float getBlockSpeed(void) const;

      float getProgressPct(void) const;
      uint64_t getETA(void) const;
      unsigned getBlocksLeft(void) const;
   };

   ////////////////////////////////////////////////////////////////////////////
   class NodeStatusStruct
   {
   private:
      std::shared_ptr<::google::protobuf::Message> msgPtr_;
      const ::Codec_NodeStatus::NodeStatus* ptr_;

   private:
      NodeStatusStruct(std::shared_ptr<::Codec_BDVCommand::BDVCallback>, unsigned);
      
   public:
      NodeStatusStruct(BinaryDataRef);
      NodeStatusStruct(std::shared_ptr<::Codec_NodeStatus::NodeStatus>);

      NodeStatus status(void) const;
      bool isSegWitEnabled(void) const;
      RpcStatus rpcStatus(void) const;
      NodeChainState chainState(void) const;

      static std::shared_ptr<NodeStatusStruct> make_new(
         std::shared_ptr<::Codec_BDVCommand::BDVCallback>, unsigned);
   };

   ////////////////////////////////////////////////////////////////////////////
   class ProgressData
   {
   private:
      std::shared_ptr<::google::protobuf::Message> msgPtr_;
      const ::Codec_NodeStatus::ProgressData* ptr_;

   private:
      ProgressData(std::shared_ptr<::Codec_BDVCommand::BDVCallback>, unsigned);

   public:
      ProgressData(BinaryDataRef);

      BDMPhase phase(void) const;
      double progress(void) const;
      unsigned time(void) const;
      unsigned numericProgress(void) const;
      std::vector<std::string> wltIDs(void) const;

      static std::shared_ptr<ProgressData> make_new(
         std::shared_ptr<::Codec_BDVCommand::BDVCallback>, unsigned);
   };
};

///////////////////////////////////////////////////////////////////////////////
struct BdmNotification
{
   const BDMAction action_;

   unsigned height_;
   unsigned branchHeight_ = UINT32_MAX;

   std::set<BinaryData> invalidatedZc_;
   std::vector<std::shared_ptr<ClientClasses::LedgerEntry>> ledgers_;

   std::vector<BinaryData> ids_;

   std::shared_ptr<::ClientClasses::NodeStatusStruct> nodeStatus_;
   BDV_Error_Struct error_;

   BdmNotification(BDMAction action) :
      action_(action)
   {}
};

///////////////////////////////////////////////////////////////////////////////
class RemoteCallback
{
public:
   RemoteCallback(void) {}
   virtual ~RemoteCallback(void) = 0;

   virtual void run(BdmNotification) = 0;
   virtual void progress(
      BDMPhase phase,
      const std::vector<std::string> &walletIdVec,
      float progress, unsigned secondsRem,
      unsigned progressNumeric
   ) = 0;
   virtual void disconnected(void) = 0;

   bool processNotifications(std::shared_ptr<::Codec_BDVCommand::BDVCallback>);
};

#endif
