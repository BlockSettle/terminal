////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016-18, goatpig.                                           //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _SERVER_H_
#define _SERVER_H_

#include <string.h>
#include <string>
#include <memory>
#include <atomic>
#include <vector>

#include "WebSocketMessage.h"
#include "libwebsockets.h"

#include "ThreadSafeClasses.h"
#include "BDV_Notification.h"
#include "BinaryData.h"
#include "EncryptionUtils.h"
#include "BlockDataManagerConfig.h"
#include "SocketService.h"


class Clients;
class BlockDataManagerThread;

///////////////////////////////////////////////////////////////////////////////
struct per_session_data__http {
   lws_fop_fd_t fop_fd;
};

struct per_session_data__bdv {
   static const unsigned rcv_size = 8000;
   uint64_t id_;
};

enum demo_protocols {
   /* always first */
   PROTOCOL_HTTP = 0,

   PROTOCOL_ARMORY_BDM,

   /* always last */
   DEMO_PROTOCOL_COUNT
};

int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
   void *in, size_t len);

///////////////////////////////////////////////////////////////////////////////
struct BDV_packet
{
   uint64_t bdvID_;
   BinaryData data_;
   struct lws *wsiPtr_;

   BDV_packet(const uint64_t& id, struct lws *wsi) :
      bdvID_(id), wsiPtr_(wsi)
   {}
};

///////////////////////////////////////////////////////////////////////////////
struct WriteStack
{
   struct lws *wsiPtr_ = nullptr;
   shared_ptr<Queue<WebSocketMessage>> stack_;
   WebSocketMessage currentMsg_;
   shared_ptr<atomic<int>> count_;

   WriteStack(struct lws *wsi) :
      wsiPtr_(wsi)
   {
      stack_ = make_shared<Queue<WebSocketMessage>>();
      count_ = make_shared<atomic<int>>();
      count_->store(0, memory_order_relaxed);
   }
};

///////////////////////////////////////////////////////////////////////////////
class WebSocketServer
{
private:
   vector<thread> threads_;
   BlockingQueue<shared_ptr<BDV_packet>> packetQueue_;
   TransactionalMap<uint64_t, WriteStack> writeMap_;

   static atomic<WebSocketServer*> instance_;
   static mutex mu_;
   static promise<bool> shutdownPromise_;
   static shared_future<bool> shutdownFuture_;
   
   unique_ptr<Clients> clients_;
   atomic<unsigned> run_;
   promise<bool> isReadyProm_;

private:
   void webSocketService(int port);
   void commandThread(void);
   void setIsReady(void);
   
   static WebSocketServer* getInstance(void);

public:
   WebSocketServer(void);

   static int callback(
      struct lws *wsi, enum lws_callback_reasons reason, 
      void *user, void *in, size_t len);

   static void start(BlockDataManagerThread* bdmT, bool async);
   static void shutdown(void);
   static void waitOnShutdown(void);

   static void write(const uint64_t&, const uint32_t&, 
      shared_ptr<::google::protobuf::Message>);
   
   shared_ptr<map<uint64_t, WriteStack>> getWriteMap(void);
   void addId(const uint64_t&, struct lws* ptr);
   void eraseId(const uint64_t&);
};

#endif
