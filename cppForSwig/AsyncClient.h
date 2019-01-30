////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2018, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/***
Handle codec and socketing for armory client
***/

#ifndef _ASYNCCLIENT_H
#define _ASYNCCLIENT_H

#include <thread>

#include "StringSockets.h"
#include "bdmenums.h"
#include "log.h"
#include "TxClasses.h"
#include "BlockDataManagerConfig.h"
#include "WebSocketClient.h"
#include "ClientClasses.h"

class WalletManager;
class WalletContainer;

///////////////////////////////////////////////////////////////////////////////
class ClientMessageError : public runtime_error
{
public:
   ClientMessageError(const string& err) :
      runtime_error(err)
   {}
};

///////////////////////////////////////////////////////////////////////////////
template<class U> class ReturnMessage
{
private:
   U value_;
   shared_ptr<ClientMessageError> error_;

public:
   ReturnMessage(void) :
      value_(U())
   {}

   ReturnMessage(U& val) :
      value_(move(val))
   {}

   ReturnMessage(const U& val) :
      value_(val)
   {}

   ReturnMessage(ClientMessageError& err)
   {
      error_ = make_shared<ClientMessageError>(err);
   }

   U get(void) 
   { 
      if (error_ != nullptr)
         throw *error_;
         
      return move(value_);
   }
};

///////////////////////////////////////////////////////////////////////////////
struct ClientCache : public Lockable
{
private:
   map<BinaryData, Tx> txMap_;
   map<unsigned, BinaryData> rawHeaderMap_;
   map<BinaryData, unsigned> txHashToHeightMap_;

public:
   void insertTx(BinaryData&, Tx&);
   void insertRawHeader(unsigned&, BinaryDataRef);
   void insertHeightForTxHash(BinaryData&, unsigned&);

   const Tx& getTx(const BinaryDataRef&) const;
   const BinaryData& getRawHeader(const unsigned&) const;
   const unsigned& getHeightForTxHash(const BinaryData&) const;

   //virtuals
   void initAfterLock(void) {}
   void cleanUpBeforeUnlock(void) {}
};

class NoMatch
{};

///////////////////////////////////////////////////////////////////////////////
namespace SwigClient
{
   class BlockDataViewer;
};

namespace AsyncClient
{
   class BlockDataViewer;

   /////////////////////////////////////////////////////////////////////////////
   class LedgerDelegate
   {
   private:
      string delegateID_;
      string bdvID_;
      shared_ptr<SocketPrototype> sock_;

   public:
      LedgerDelegate(void) {}

      LedgerDelegate(shared_ptr<SocketPrototype>, const string&, const string&);

      void getHistoryPage(uint32_t id, 
         function<void(ReturnMessage<vector<::ClientClasses::LedgerEntry>>)>);
      void getPageCount(function<void(ReturnMessage<uint64_t>)>) const;
   };

   class BtcWallet;

   /////////////////////////////////////////////////////////////////////////////
   class ScrAddrObj
   {
      friend class ::WalletContainer;

   private:
      const string bdvID_;
      const string walletID_;
      const BinaryData scrAddr_;
      BinaryData addrHash_;
      const shared_ptr<SocketPrototype> sock_;

      const uint64_t fullBalance_;
      const uint64_t spendableBalance_;
      const uint64_t unconfirmedBalance_;
      const uint32_t count_;
      const int index_;

      string comment_;

   private:
      ScrAddrObj(const BinaryData& addr, const BinaryData& addrHash, int index) :
         bdvID_(string()), walletID_(string()), 
         scrAddr_(addr), addrHash_(addrHash),
         sock_(nullptr), 
         fullBalance_(0), spendableBalance_(0), unconfirmedBalance_(0),
         count_(0), index_(index)
      {}

   public:
      ScrAddrObj(BtcWallet*, const BinaryData&, int index,
         uint64_t, uint64_t, uint64_t, uint32_t);
      ScrAddrObj(shared_ptr<SocketPrototype>,
         const string&, const string&, const BinaryData&, int index,
         uint64_t, uint64_t, uint64_t, uint32_t);

      uint64_t getFullBalance(void) const { return fullBalance_; }
      uint64_t getSpendableBalance(void) const { return spendableBalance_; }
      uint64_t getUnconfirmedBalance(void) const { return unconfirmedBalance_; }

      uint64_t getTxioCount(void) const { return count_; }

      void getSpendableTxOutList(bool, function<void(ReturnMessage<vector<UTXO>>)>);
      const BinaryData& getScrAddr(void) const { return scrAddr_; }
      const BinaryData& getAddrHash(void) const { return addrHash_; }

      void setComment(const string& comment) { comment_ = comment; }
      const string& getComment(void) const { return comment_; }
      int getIndex(void) const { return index_; }
   };

   /////////////////////////////////////////////////////////////////////////////
   class BtcWallet
   {
      friend class ScrAddrObj;

   protected:
      const string walletID_;
      const string bdvID_;
      const shared_ptr<SocketPrototype> sock_;

   public:
      BtcWallet(const BlockDataViewer&, const string&);
      
      void getBalancesAndCount(uint32_t topBlockHeight,
         function<void(ReturnMessage<vector<uint64_t>>)>);

      void getSpendableTxOutListForValue(uint64_t val, 
         function<void(ReturnMessage<vector<UTXO>>)>);
      void getSpendableZCList(function<void(ReturnMessage<vector<UTXO>>)>);
      void getRBFTxOutList(function<void(ReturnMessage<vector<UTXO>>)>);

      void getAddrTxnCountsFromDB(function<void(
         ReturnMessage<map<BinaryData, uint32_t>>)>);
      void getAddrBalancesFromDB(function<void(
         ReturnMessage<map<BinaryData, vector<uint64_t>>>)>);

      void getHistoryPage(uint32_t id, 
         function<void(ReturnMessage<vector<::ClientClasses::LedgerEntry>>)>);
      void getLedgerEntryForTxHash(
         const BinaryData& txhash, 
         function<void(ReturnMessage<shared_ptr<::ClientClasses::LedgerEntry>>)>);

      ScrAddrObj getScrAddrObjByKey(const BinaryData&,
         uint64_t, uint64_t, uint64_t, uint32_t);

      virtual string registerAddresses(
         const vector<BinaryData>& addrVec, bool isNew);
      void createAddressBook(
         function<void(ReturnMessage<vector<AddressBookEntry>>)>) const;
   };

   /////////////////////////////////////////////////////////////////////////////
   class Lockbox : public BtcWallet
   {
   private:
      uint64_t fullBalance_ = 0;
      uint64_t spendableBalance_ = 0;
      uint64_t unconfirmedBalance_ = 0;

      uint64_t txnCount_ = 0;

   public:

      Lockbox(const BlockDataViewer& bdv, const string& id) :
         BtcWallet(bdv, id)
      {}

      void getBalancesAndCountFromDB(uint32_t topBlockHeight);

      uint64_t getFullBalance(void) const { return fullBalance_; }
      uint64_t getSpendableBalance(void) const { return spendableBalance_; }
      uint64_t getUnconfirmedBalance(void) const { return unconfirmedBalance_; }
      uint64_t getWltTotalTxnCount(void) const { return txnCount_; }
 
      string registerAddresses(
         const vector<BinaryData>& addrVec, bool isNew);
   };

   /////////////////////////////////////////////////////////////////////////////
   class Blockchain
   {
   private:
      const shared_ptr<SocketPrototype> sock_;
      const string bdvID_;

   public:
      Blockchain(const BlockDataViewer&);
      void getHeaderByHash(const BinaryData& hash, 
         function<void(ReturnMessage<ClientClasses::BlockHeader>)>);
      void getHeaderByHeight(
         unsigned height, 
         function<void(ReturnMessage<ClientClasses::BlockHeader>)>);
   };

   /////////////////////////////////////////////////////////////////////////////
   class BlockDataViewer
   {
      friend class ScrAddrObj;
      friend class BtcWallet;
      friend class RemoteCallback;
      friend class LedgerDelegate;
      friend class Blockchain;
      friend class ::WalletManager;
      friend class SwigClient::BlockDataViewer;

   private:
      string bdvID_;
      shared_ptr<SocketPrototype> sock_;
      shared_ptr<ClientCache> cache_;

   private:
      BlockDataViewer(void);
      BlockDataViewer(shared_ptr<SocketPrototype> sock);
      bool isValid(void) const { return sock_ != nullptr; }

      const BlockDataViewer& operator=(const BlockDataViewer& rhs)
      {
         bdvID_ = rhs.bdvID_;
         sock_ = rhs.sock_;
         cache_ = rhs.cache_;

         return *this;
      }

   public:
      ~BlockDataViewer(void);

      bool connectToRemote(void);
      BtcWallet instantiateWallet(const string& id);
      Lockbox instantiateLockbox(const string& id);

      const string& getID(void) const { return bdvID_; }
      shared_ptr<SocketPrototype> getSocketObject(void) const { return sock_; }

      static shared_ptr<BlockDataViewer> getNewBDV(
         const string& addr, const string& port, shared_ptr<RemoteCallback>);

      void getLedgerDelegateForWallets(
         function<void(ReturnMessage<LedgerDelegate>)>);
      void getLedgerDelegateForLockboxes(
         function<void(ReturnMessage<LedgerDelegate>)>);
      void getLedgerDelegateForScrAddr(
         const string&, const BinaryData&, 
         function<void(ReturnMessage<LedgerDelegate>)>);
      Blockchain blockchain(void);

      void goOnline(void);
      void registerWithDB(BinaryData magic_word);
      void unregisterFromDB(void);
      void shutdown(const string&);
      void shutdownNode(const string&);

      void broadcastZC(const BinaryData& rawTx);
      void getTxByHash(const BinaryData& txHash, 
         function<void(ReturnMessage<Tx>)>);
      void getRawHeaderForTxHash(
         const BinaryData& txHash, 
         function<void(ReturnMessage<BinaryData>)>);
      void getHeaderByHeight(
         unsigned height, 
         function<void(ReturnMessage<BinaryData>)>);

      void updateWalletsLedgerFilter(const vector<BinaryData>& wltIdVec);
      bool hasRemoteDB(void);

      void getNodeStatus(
         function<void(ReturnMessage<shared_ptr<::ClientClasses::NodeStatusStruct>>)>);
      void estimateFee(unsigned, const string&, 
         function<void(ReturnMessage<ClientClasses::FeeEstimateStruct>)>);
      void getFeeSchedule(const string&,function<void(ReturnMessage<
            std::map<unsigned, ClientClasses::FeeEstimateStruct>>)>);

      void getHistoryForWalletSelection(
         const vector<string>& wldIDs, const string& orderingStr,
         function<void(ReturnMessage<vector<::ClientClasses::LedgerEntry>>)>);

      void broadcastThroughRPC(const BinaryData& rawTx, 
         function<void(ReturnMessage<string>)>);

      void getUtxosForAddrVec(const vector<BinaryData>&, 
         function<void(ReturnMessage<vector<UTXO>>)>);

      static unique_ptr<WritePayload_Protobuf> make_payload(
         ::Codec_BDVCommand::Methods, const string&);
      static unique_ptr<WritePayload_Protobuf> make_payload(
         ::Codec_BDVCommand::StaticMethods);
   };

   ////////////////////////////////////////////////////////////////////////////
   void deserialize(::google::protobuf::Message*, 
      const WebSocketMessagePartial&);
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//// callback structs for async networking
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_BinaryDataRef : public CallbackReturn_WebSocket
{
private:
   function<void(BinaryDataRef)> userCallbackLambda_;

public:
   CallbackReturn_BinaryDataRef(function<void(BinaryDataRef)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_String : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<string>)> userCallbackLambda_;

public:
   CallbackReturn_String(function<void(ReturnMessage<string>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_LedgerDelegate : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<AsyncClient::LedgerDelegate>)> userCallbackLambda_;
   shared_ptr<SocketPrototype> sockPtr_;
   const string& bdvID_;

public:
   CallbackReturn_LedgerDelegate(
      shared_ptr<SocketPrototype> sock, const string& bdvid,
      function<void(ReturnMessage<AsyncClient::LedgerDelegate>)> lbd) :
      userCallbackLambda_(lbd), sockPtr_(sock), bdvID_(bdvid)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_Tx : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<Tx>)> userCallbackLambda_;
   shared_ptr<ClientCache> cache_;
   BinaryData txHash_;

public:
   CallbackReturn_Tx(shared_ptr<ClientCache> cache,
      const BinaryData& txHash, function<void(ReturnMessage<Tx>)> lbd) :
      userCallbackLambda_(lbd), cache_(cache), txHash_(txHash)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_RawHeader : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<BinaryData>)> userCallbackLambda_;
   shared_ptr<ClientCache> cache_;
   BinaryData txHash_;
   unsigned height_;

public:
   CallbackReturn_RawHeader(
      shared_ptr<ClientCache> cache,
      unsigned height, const BinaryData& txHash, 
      function<void(ReturnMessage<BinaryData>)> lbd) :
      userCallbackLambda_(lbd),
      cache_(cache),txHash_(txHash), height_(height)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
class CallbackReturn_NodeStatusStruct : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<shared_ptr<::ClientClasses::NodeStatusStruct>>)>
      userCallbackLambda_;

public:
   CallbackReturn_NodeStatusStruct(function<void(
      ReturnMessage<shared_ptr<::ClientClasses::NodeStatusStruct>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_FeeEstimateStruct : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<ClientClasses::FeeEstimateStruct>)> 
      userCallbackLambda_;

public:
   CallbackReturn_FeeEstimateStruct(
      function<void(ReturnMessage<ClientClasses::FeeEstimateStruct>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_FeeSchedule : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<map<unsigned, ClientClasses::FeeEstimateStruct>>)>
      userCallbackLambda_;

public:
   CallbackReturn_FeeSchedule(function<void(ReturnMessage<
      map<unsigned, ClientClasses::FeeEstimateStruct>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_VectorLedgerEntry : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<vector<::ClientClasses::LedgerEntry>>)> 
      userCallbackLambda_;

public:
   CallbackReturn_VectorLedgerEntry(
      function<void(ReturnMessage<vector<::ClientClasses::LedgerEntry>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_UINT64 : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<uint64_t>)> userCallbackLambda_;

public:
   CallbackReturn_UINT64(
      function<void(ReturnMessage<uint64_t>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_VectorUTXO : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<vector<UTXO>>)> userCallbackLambda_;

public:
   CallbackReturn_VectorUTXO(
      function<void(ReturnMessage<vector<UTXO>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_VectorUINT64 : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<vector<uint64_t>>)> userCallbackLambda_;

public:
   CallbackReturn_VectorUINT64(
      function<void(ReturnMessage<vector<uint64_t>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_Map_BD_U32 : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<map<BinaryData, uint32_t>>)> userCallbackLambda_;

public:
   CallbackReturn_Map_BD_U32(
      function<void(ReturnMessage<map<BinaryData, uint32_t>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_Map_BD_VecU64 : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<map<BinaryData, vector<uint64_t>>>)> 
      userCallbackLambda_;

public:
   CallbackReturn_Map_BD_VecU64(
      function<void(ReturnMessage<map<BinaryData, vector<uint64_t>>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_LedgerEntry : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<shared_ptr<::ClientClasses::LedgerEntry>>)> 
      userCallbackLambda_;

public:
   CallbackReturn_LedgerEntry(
      function<void(ReturnMessage<shared_ptr<::ClientClasses::LedgerEntry>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_VectorAddressBookEntry : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<vector<AddressBookEntry>>)> userCallbackLambda_;

public:
   CallbackReturn_VectorAddressBookEntry(
      function<void(ReturnMessage<vector<AddressBookEntry>>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_Bool : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<bool>)> userCallbackLambda_;

public:
   CallbackReturn_Bool(function<void(ReturnMessage<bool>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_BlockHeader : public CallbackReturn_WebSocket
{
private:
   function<void(ReturnMessage<ClientClasses::BlockHeader>)> userCallbackLambda_;
   const unsigned height_;

public:
   CallbackReturn_BlockHeader(unsigned height, 
      function<void(ReturnMessage<ClientClasses::BlockHeader>)> lbd) :
      userCallbackLambda_(lbd), height_(height)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

///////////////////////////////////////////////////////////////////////////////
struct CallbackReturn_BDVCallback : public CallbackReturn_WebSocket
{
private:
   function<void(shared_ptr<::Codec_BDVCommand::BDVCallback>)>
      userCallbackLambda_;

public:
   CallbackReturn_BDVCallback(
      function<void(shared_ptr<::Codec_BDVCommand::BDVCallback>)> lbd) :
      userCallbackLambda_(lbd)
   {}

   //virtual
   void callback(const WebSocketMessagePartial&);
};

#endif
