////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _BDM_SERVER_H
#define _BDM_SERVER_H

#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <future>

#include "BitcoinP2p.h"
#include "BlockDataViewer.h"
#include "EncryptionUtils.h"
#include "LedgerEntry.h"
#include "DbHeader.h"
#include "BDV_Notification.h"
#include "BDVCodec.h"
#include "ZeroConf.h"
#include "Server.h"
#include "BtcWallet.h"

#define MAX_CONTENT_LENGTH 1024*1024*1024
#define CALLBACK_EXPIRE_COUNT 5

enum WalletType
{
   TypeWallet,
   TypeLockbox
};

class BDV_Server_Object;

namespace DBTestUtils
{
   tuple<shared_ptr<::Codec_BDVCommand::BDVCallback>, unsigned> waitOnSignal(
      Clients*, const string&, ::Codec_BDVCommand::NotificationType);
}

///////////////////////////////////////////////////////////////////////////////
struct BDV_Payload
{
   shared_ptr<BDV_packet> packet_;
   shared_ptr<BDV_Server_Object> bdvPtr_;
   uint32_t messageID_;
   size_t packetID_;
};

///////////////////////////////////////////////////////////////////////////////
struct BDV_PartialMessage
{
   vector<shared_ptr<BDV_Payload>> payloads_;
   WebSocketMessagePartial partialMessage_;

   bool parsePacket(shared_ptr<BDV_Payload>);
   bool isReady(void) const { return partialMessage_.isReady(); }
   bool getMessage(shared_ptr<::google::protobuf::Message>);
   void reset(void);
   size_t topId(void) const;
};

///////////////////////////////////////////////////////////////////////////////
class Callback
{
public:

   virtual ~Callback() = 0;

   virtual void callback(shared_ptr<::Codec_BDVCommand::BDVCallback>) = 0;
   virtual bool isValid(void) = 0;
   virtual void shutdown(void) = 0;
};

///////////////////////////////////////////////////////////////////////////////
class WS_Callback : public Callback
{
private:
   const uint64_t bdvID_;

public:
   WS_Callback(const uint64_t& bdvid) :
      bdvID_(bdvid)
   {}

   void callback(shared_ptr<::Codec_BDVCommand::BDVCallback>);
   bool isValid(void) { return true; }
   void shutdown(void) {}
};

///////////////////////////////////////////////////////////////////////////////
class UnitTest_Callback : public Callback
{
private:
   BlockingQueue<shared_ptr<::Codec_BDVCommand::BDVCallback>> notifQueue_;

public:
   void callback(shared_ptr<::Codec_BDVCommand::BDVCallback>);
   bool isValid(void) { return true; }
   void shutdown(void) {}

   shared_ptr<::Codec_BDVCommand::BDVCallback> getNotification(void);
};

///////////////////////////////////////////////////////////////////////////////
class BDV_Server_Object : public BlockDataViewer
{
   friend class Clients;
   friend std::tuple<std::shared_ptr<::Codec_BDVCommand::BDVCallback>, unsigned>
      DBTestUtils::waitOnSignal(
      Clients*, const std::string&, ::Codec_BDVCommand::NotificationType);

private: 
   std::thread initT_;
   std::unique_ptr<Callback> cb_;

   std::string bdvID_;
   BlockDataManagerThread* bdmT_;

   std::map<std::string, LedgerDelegate> delegateMap_;

   struct walletRegStruct
   {
      std::shared_ptr<::Codec_BDVCommand::BDVCommand> command_;
      WalletType type_;
   };

   std::mutex registerWalletMutex_;
   std::map<std::string, walletRegStruct> wltRegMap_;

   std::shared_ptr<std::promise<bool>> isReadyPromise_;
   std::shared_future<bool> isReadyFuture_;

   std::function<void(std::unique_ptr<BDV_Notification>)> notifLambda_;
   std::atomic<unsigned> packetProcess_threadLock_;
   std::atomic<unsigned> notificationProcess_threadLock_;

   std::map<size_t, std::shared_ptr<BDV_Payload>> packetMap_;
   BDV_PartialMessage currentMessage_;
   std::atomic<size_t> nextPacketId_ = {0};
   std::shared_ptr<BDV_Payload> packetToReinject_ = nullptr;

private:
   BDV_Server_Object(BDV_Server_Object&) = delete; //no copies
      
   std::shared_ptr<::google::protobuf::Message> processCommand(
      std::shared_ptr<::Codec_BDVCommand::BDVCommand>);
   void startThreads(void);

   void registerWallet(std::shared_ptr<::Codec_BDVCommand::BDVCommand>);
   void registerLockbox(std::shared_ptr<::Codec_BDVCommand::BDVCommand>);
   void populateWallets(std::map<std::string, walletRegStruct>&);
   void setup(void);

   void flagRefresh(
      BDV_refresh refresh, const BinaryData& refreshId,
      std::unique_ptr<BDV_Notification_ZC> zcPtr);
   void resetCurrentMessage(void);

public:
   BDV_Server_Object(const std::string& id, BlockDataManagerThread *bdmT);

   ~BDV_Server_Object(void) 
   { 
      haltThreads(); 
   }

   const std::string& getID(void) const { return bdvID_; }
   void processNotification(std::shared_ptr<BDV_Notification>);
   void init(void);
   void haltThreads(void);
   bool processPayload(std::shared_ptr<BDV_Payload>&,
      std::shared_ptr<::google::protobuf::Message>&);

   size_t getNextPacketId(void) { return nextPacketId_.fetch_add(1, std::memory_order_relaxed); }
};

class Clients;

///////////////////////////////////////////////////////////////////////////////
class ZeroConfCallbacks_BDV : public ZeroConfCallbacks
{
private:
   Clients * clientsPtr_;

public:
   ZeroConfCallbacks_BDV(Clients* clientsPtr) :
      clientsPtr_(clientsPtr)
   {}

   set<string> hasScrAddr(const BinaryDataRef&) const;
   void pushZcNotification(ZeroConfContainer::NotificationPacket& packet);
   void errorCallback(
      const std::string& bdvId, std::string& errorStr, const std::string& txHash);
};

///////////////////////////////////////////////////////////////////////////////
class Clients
{
   friend class ZeroConfCallbacks_BDV;

private:
   TransactionalMap<std::string, shared_ptr<BDV_Server_Object>> BDVs_;
   mutable BlockingQueue<bool> gcCommands_;
   BlockDataManagerThread* bdmT_ = nullptr;

   std::function<void(void)> shutdownCallback_;

   std::atomic<bool> run_;

   std::vector<std::thread> controlThreads_;

   mutable BlockingQueue<std::shared_ptr<BDV_Notification>> outerBDVNotifStack_;
   BlockingQueue<std::shared_ptr<BDV_Notification_Packet>> innerBDVNotifStack_;
   BlockingQueue<std::shared_ptr<BDV_Payload>> packetQueue_;

   mutex shutdownMutex_;

private:
   void notificationThread(void) const;
   void unregisterAllBDVs(void);
   void bdvMaintenanceLoop(void);
   void bdvMaintenanceThread(void);
   void messageParserThread(void);

public:

   Clients(void)
   {}

   Clients(BlockDataManagerThread* bdmT,
      std::function<void(void)> shutdownLambda)
   {
      init(bdmT, shutdownLambda);
   }

   void init(BlockDataManagerThread* bdmT,
      std::function<void(void)> shutdownLambda);

   std::shared_ptr<BDV_Server_Object> get(const std::string& id) const;
   
   void processShutdownCommand(
      std::shared_ptr<::Codec_BDVCommand::StaticCommand>);
   std::shared_ptr<::google::protobuf::Message> registerBDV(
      std::shared_ptr<::Codec_BDVCommand::StaticCommand>, string bdvID);
   void unregisterBDV(const std::string& bdvId);
   void shutdown(void);
   void exitRequestLoop(void);
   
   void queuePayload(std::shared_ptr<BDV_Payload>& payload)
   {  
      packetQueue_.push_back(move(payload));
   }

   std::shared_ptr<::google::protobuf::Message> processUnregisteredCommand(
      const uint64_t& bdvId, std::shared_ptr<::Codec_BDVCommand::StaticCommand>);
   std::shared_ptr<::google::protobuf::Message> processCommand(
      std::shared_ptr<BDV_Payload>);
};

#endif
