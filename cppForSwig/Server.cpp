////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016-18, goatpig.                                           //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////


#include "Server.h"
#include "BlockDataManagerConfig.h"
#include "BDM_Server.h"

using namespace std;
using namespace ::google::protobuf;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//// WebSocketServer
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
atomic<WebSocketServer*> WebSocketServer::instance_;
mutex WebSocketServer::mu_;
promise<bool> WebSocketServer::shutdownPromise_;
shared_future<bool> WebSocketServer::shutdownFuture_;
BinaryData WebSocketServer::encInitPacket_ = READHEX("010000000B");

///////////////////////////////////////////////////////////////////////////////
WebSocketServer::WebSocketServer()
{
   clients_ = make_unique<Clients>();
}

///////////////////////////////////////////////////////////////////////////////
static struct lws_protocols protocols[] = {
   /* first protocol must always be HTTP handler */

   {
      "http-only",		/* name */
      callback_http,		/* callback */
      sizeof(struct per_session_data__http),	/* per_session_data_size */
      0,			/* max frame size / rx buffer */
   },
   {
      "armory-bdm-protocol",
      WebSocketServer::callback,
      sizeof(struct per_session_data__bdv),
      per_session_data__bdv::rcv_size,
   },

{ NULL, NULL, 0, 0 } /* terminator */
};

///////////////////////////////////////////////////////////////////////////////
int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
   void *user, void *in, size_t len)
{
   return 0;
}

///////////////////////////////////////////////////////////////////////////////
int WebSocketServer::callback(
   struct lws *wsi, enum lws_callback_reasons reason,
   void *user, void *in, size_t len)
{
   struct per_session_data__bdv *session_data =
      (struct per_session_data__bdv *)user;

   switch (reason)
   {

   case LWS_CALLBACK_PROTOCOL_INIT:
   {
      auto instance = WebSocketServer::getInstance();
      instance->setIsReady();
      break;
   }

   case LWS_CALLBACK_ESTABLISHED:
   {
      auto&& bdid = CryptoPRNG::generateRandom(8);
      session_data->id_ = *(uint64_t*)bdid.getPtr();

      auto instance = WebSocketServer::getInstance();
      instance->addId(session_data->id_, wsi);
      instance->processAEADHandshake(
         session_data->id_, encInitPacket_, true);

      break;
   }

   case LWS_CALLBACK_CLOSED:
   {
      auto instance = WebSocketServer::getInstance();
      BinaryDataRef bdr((uint8_t*)&session_data->id_, 8);
      instance->clients_->unregisterBDV(bdr.toHexStr());
      instance->eraseId(session_data->id_);

      break;
   }

   case LWS_CALLBACK_RECEIVE:
   {
      auto packetPtr = make_shared<BDV_packet>(session_data->id_, wsi);
      packetPtr->data_.resize(len);
      memcpy(packetPtr->data_.getPtr(), (uint8_t*)in, len);

      auto wsPtr = WebSocketServer::getInstance();
      wsPtr->packetQueue_.push_back(move(packetPtr));
      break;
   }

   case LWS_CALLBACK_SERVER_WRITEABLE:
   {
      auto wsPtr = WebSocketServer::getInstance();
      auto stateMap = wsPtr->getConnectionStateMap();
      auto iter = stateMap->find(session_data->id_);
      if (iter == stateMap->end())
      {
         //no connection state object, kill this connection
         return -1;
      }

      if (iter->second.closeFlag_->load(memory_order_relaxed) == -1)
      {
         //connection flagged for closing
         return -1;
      }

      auto& stateObj = iter->second;
      if (stateObj.currentMsg_.isDone())
      {
         try
         {
            stateObj.currentMsg_ =
               move(stateObj.serializedStack_->pop_front());
         }
         catch (IsEmpty&)
         {
            auto val = iter->second.count_->load(memory_order_relaxed);
            if(val != 0)
               LOGWARN << "!!!! out of server write loop with pending message: " << val;
            break;
         }
      }
      
      auto& ws_msg = stateObj.currentMsg_;

      auto& packet = ws_msg.getNextPacket();
      auto body = (uint8_t*)packet.getPtr() + LWS_PRE;

      auto m = lws_write(wsi, 
         body, packet.getSize() - LWS_PRE,
         LWS_WRITE_BINARY);

      if (m != (int)packet.getSize() - (int)LWS_PRE)
      {
         LOGERR << "failed to send packet of size";
         LOGERR << "packet is " << packet.getSize() <<
            " bytes, sent " << m << " bytes";
      }

      if (stateObj.currentMsg_.isDone())
         stateObj.count_->fetch_sub(1, memory_order_relaxed);
      /***
      In case several threads are trying to write to the same socket, it's
      possible their calls to callback_on_writeable may overlap, resulting 
      in a single write entry being consumed.

      To avoid this, we trigger the callback from within itself, which will 
      break out if there are no more items in the writeable stack.
      ***/
      lws_callback_on_writable(wsi);

      break;
   }

   default:
      break;

   }

   return 0;
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::start(BlockDataManagerThread* bdmT, bool async)
{
   shutdownPromise_ = promise<bool>();
   shutdownFuture_ = shutdownPromise_.get_future();
   auto instance = getInstance();

   //init auth peer object
   string peerFilename(SERVER_AUTH_PEER_FILENAME);
   instance->authorizedPeers_ = make_shared<AuthorizedPeers>(
      BlockDataManagerConfig::getDataDir(), peerFilename);

   //init Clients object
   auto shutdownLbd = [](void)->void
   {
      WebSocketServer::shutdown();
   };

   instance->clients_->init(bdmT, shutdownLbd);

   //start command threads
   auto commandThr = [instance](void)->void
   {
      instance->commandThread();
   };

   instance->threads_.push_back(thread(commandThr));

   //start write threads
   auto writeProcessThread = [instance](void)->void
   {
      instance->prepareWriteThread();
   };

   for(unsigned i=0; i<3; i++)
      instance->threads_.push_back(thread(writeProcessThread));

   auto port = stoi(bdmT->bdm()->config().listenPort_);
   if (port == 0)
      port = WEBSOCKET_PORT;

   //run service thread
   if (async)
   {
      auto loopthr = [instance, port](void)->void
      {
         instance->webSocketService(port);
      };

      auto fut = instance->isReadyProm_.get_future();
      instance->threads_.push_back(thread(loopthr));

      fut.get();
      return;
   }

   instance->webSocketService(port);
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::shutdown()
{
   unique_lock<mutex> lock(mu_, defer_lock);
   if (!lock.try_lock())
      return;
   
   auto ptr = instance_.load(memory_order_relaxed);
   if (ptr == nullptr)
      return;

   auto instance = getInstance();
   if (instance->run_.load(memory_order_relaxed) == 0)
      return;

   instance->msgQueue_.terminate();
   instance->clients_->shutdown();
   instance->run_.store(0, memory_order_relaxed);
   instance->packetQueue_.terminate();

   vector<thread::id> idVec;
   for (auto& thr : instance->threads_)
   {
      idVec.push_back(thr.get_id());
      if (thr.joinable())
         thr.join();
   }

   instance->threads_.clear();
   DatabaseContainer_Sharded::clearThreadShardTx(idVec);

   instance_.store(nullptr, memory_order_relaxed);
   delete instance;
   
   try
   {
      shutdownPromise_.set_value(true);
   }
   catch (future_error)
   {}
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::setIsReady()
{
   try
   {
      isReadyProm_.set_value(true);
   }
   catch (future_error&)
   {}
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::webSocketService(int port)
{
   struct lws_context_creation_info info;
   struct lws_vhost *vhost;
   const char *iface = nullptr;
   int uid = -1, gid = -1;
   int pp_secs = 0;
   int opts = 0;
   int n = 0;

   memset(&info, 0, sizeof info);
   info.port = port;

   info.iface = iface;
   info.protocols = protocols;
   info.log_filepath = nullptr;
   info.ws_ping_pong_interval = pp_secs;
   info.gid = gid;
   info.uid = uid;
   info.max_http_header_pool = 256;
   info.options = opts | LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
   info.timeout_secs = 0;
   info.ip_limit_ah = 24; /* for testing */
   info.ip_limit_wsi = 105; /* for testing */
   
   auto context = lws_create_context(&info);
   if (context == nullptr) 
      throw LWS_Error("failed to create LWS context");

   vhost = lws_create_vhost(context, &info);
   if (vhost == nullptr)
      throw LWS_Error("failed to create vhost");

   run_.store(1, memory_order_relaxed);
   try
   {
      while (run_.load(memory_order_relaxed) != 0 && n >= 0)
      {
         n = lws_service(context, 50);
      }
   }
   catch(exception& e)
   {
      LOGERR << "server lws service choked: " << e.what();
   }

   LOGINFO << "cleaning up lws server";
   lws_vhost_destroy(vhost);
   lws_context_destroy(context);
}

///////////////////////////////////////////////////////////////////////////////
WebSocketServer* WebSocketServer::getInstance()
{
   while (1)
   {
      auto ptr = instance_.load(memory_order_relaxed);
      if (ptr == nullptr)
      {
         unique_lock<mutex> lock(mu_);
         ptr = instance_.load(memory_order_relaxed);
         if (ptr != nullptr)
            continue;

         ptr = new WebSocketServer();
         instance_.store(ptr, memory_order_relaxed);
      }

      return ptr;
   }
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::commandThread()
{
   while (1)
   {
      shared_ptr<BDV_packet> packetPtr;
      try
      {
         packetPtr = move(packetQueue_.pop_front());
      }
      catch (StopBlockingLoop&)
      {
         //end loop condition
         return;
      }

      if (packetPtr == nullptr)
      {
         LOGWARN << "empty command packet";
         continue;
      }

      //check wsi is valid
      if (packetPtr->wsiPtr_ == nullptr)
      {
         LOGWARN << "null wsi";
         continue;
      }

      //get connection state object
      auto stateMap = getConnectionStateMap();
      auto iter = stateMap->find(packetPtr->bdvID_);
      if (iter == stateMap->end())
      {
         //missing state map, kill connection
         continue;
      }

      auto& bip151Connection = iter->second.bip151Connection_;
      if (bip151Connection->connectionComplete())
      {
         //decrypt packet
         size_t plainTextSize = packetPtr->data_.getSize() - POLY1305MACLEN;
         if (bip151Connection->decryptPacket(
            packetPtr->data_.getPtr(), packetPtr->data_.getSize(),
            (uint8_t*)packetPtr->data_.getPtr(), packetPtr->data_.getSize()) != 0)
         {
            //failed to decrypt, kill connection
            closeClientConnection(packetPtr->bdvID_);
            continue;
         }

         packetPtr->data_.resize(plainTextSize);
      }

      uint8_t msgType = 
         WebSocketMessagePartial::getPacketType(packetPtr->data_.getRef());

      if (msgType > WS_MSGTYPE_AEAD_THESHOLD)
      {
         processAEADHandshake(packetPtr->bdvID_, move(packetPtr->data_), false);
         continue;
      }

      if (bip151Connection->getBIP150State() != BIP150State::SUCCESS)
      {
         //can't get this far without fully setup AEAD
         closeClientConnection(packetPtr->bdvID_);
         continue;
      }

      BinaryDataRef bdr((uint8_t*)&packetPtr->bdvID_, 8);
      auto&& hexID = bdr.toHexStr();
      auto bdvPtr = clients_->get(hexID);

      if (bdvPtr != nullptr)
      {
         //create payload
         auto bdv_payload = make_shared<BDV_Payload>();
         bdv_payload->bdvPtr_ = bdvPtr;
         bdv_payload->packet_ = packetPtr;
  
         //queue for clients thread pool to process
         clients_->queuePayload(bdv_payload);
      }
      else
      {
         //unregistered command
         WebSocketMessagePartial msgObj;
         msgObj.parsePacket(packetPtr->data_);
         if (msgObj.getType() != WS_MSGTYPE_SINGLEPACKET)
         {
            //invalid msg type, kill connection
            continue;
         }

         auto&& messageRef = msgObj.getSingleBinaryMessage();
            
         if (messageRef.getSize() == 0)
         {
            //invalid msg, kill connection
            continue;
         }

         //process command 
         auto message = make_shared<::Codec_BDVCommand::StaticCommand>();
         if (!message->ParseFromArray(messageRef.getPtr(), messageRef.getSize()))
         {
            //invalid msg, kill connection
            continue;
         }

         auto&& reply = clients_->processUnregisteredCommand(
            packetPtr->bdvID_, message);

         //reply
         write(packetPtr->bdvID_, msgObj.getId(), reply);
      }
   }

   DatabaseContainer_Sharded::clearThreadShardTx(this_thread::get_id());
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::write(const uint64_t& id, const uint32_t& msgid,
   shared_ptr<Message> message)
{
   if (message == nullptr)
      return;

   auto msg = make_unique<PendingMessage>(id, msgid, message);
   auto instance = getInstance();
   instance->msgQueue_.push_back(move(msg));
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::prepareWriteThread()
{
   while (true)
   {
      unique_ptr<PendingMessage> msg;
      try
      {
         msg = msgQueue_.pop_front();
      }
      catch (StopBlockingLoop&)
      {
         break;
      }

      if (msg == nullptr)
         continue;

      auto statemap = getConnectionStateMap();
      auto stateIter = statemap->find(msg->id_);
      if (stateIter == statemap->end())
         continue;
      auto statePtr = &stateIter->second;

      //grab state object lock
      unsigned zero = 0;
      if(!statePtr->lock_->compare_exchange_weak(zero, 1))
      { 
         msgQueue_.push_back(move(msg));
         continue;
      }

      if (!statePtr->bip151Connection_->connectionComplete())
      {
         //aead session uninitialized, kill connection
         return;
      }

      //check for rekey
      {
         bool needs_rekey = false;
         auto rightnow = chrono::system_clock::now();

         if (statePtr->bip151Connection_->rekeyNeeded(msg->message_->ByteSize()))
         {
            needs_rekey = true;
         }
         else
         {
            auto time_sec = chrono::duration_cast<chrono::seconds>(
               rightnow - statePtr->outKeyTimePoint_);
            if (time_sec.count() >= AEAD_REKEY_INVERVAL_SECONDS)
               needs_rekey = true;
         }
         
         if (needs_rekey)
         {
            //create rekey packet
            BinaryData rekeyPacket(BIP151PUBKEYSIZE);
            memset(rekeyPacket.getPtr(), 0, BIP151PUBKEYSIZE);
            
            SerializedMessage ws_msg;
            ws_msg.construct(
               rekeyPacket.getDataVector(), 
               statePtr->bip151Connection_.get(),
               WS_MSGTYPE_AEAD_REKEY);

            //push to write map
            statePtr->serializedStack_->push_back(move(ws_msg));

            //rekey outer bip151 channel
            statePtr->bip151Connection_->rekeyOuterSession();

            //set outkey timepoint to rightnow
            statePtr->outKeyTimePoint_ = rightnow;
         }
      }

      //serialize arg
      vector<uint8_t> serializedData;
      if (msg->message_->ByteSize() > 0)
      {
         serializedData.resize(msg->message_->ByteSize());
         auto result = msg->message_->SerializeToArray(
            &serializedData[0], serializedData.size());
         if (!result)
         {
            LOGWARN << "failed to serialize message";
            return;
         }
      }

      SerializedMessage ws_msg;
      ws_msg.construct(
         serializedData, statePtr->bip151Connection_.get(), 
         WS_MSGTYPE_FRAGMENTEDPACKET_HEADER, msg->msgid_);

      //push to write map
      statePtr->serializedStack_->push_back(move(ws_msg));
      statePtr->count_->fetch_add(1, memory_order_relaxed);

      //reset lock
      statePtr->lock_->store(0);

      //call write callback
      lws_callback_on_writable(statePtr->wsiPtr_);
   }
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::waitOnShutdown()
{
   try
   {
      shutdownFuture_.get();
   }
   catch(future_error&)
   { }
}

///////////////////////////////////////////////////////////////////////////////
shared_ptr<map<uint64_t, ClientConnectionState>> 
   WebSocketServer::getConnectionStateMap() const
{
   return clientStateMap_.get();
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::addId(const uint64_t& id, struct lws* ptr)
{
   auto&& lbds = getAuthPeerLambda();
   auto&& write_pair = make_pair(id, ClientConnectionState(ptr, lbds));
   clientStateMap_.insert(move(write_pair));
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::eraseId(const uint64_t& id)
{
   clientStateMap_.erase(id);
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::processAEADHandshake(
   uint64_t id, BinaryData msg, bool async)
{
   auto writeToClient = [](
      ClientConnectionState& ccs, uint8_t type, 
      const vector<uint8_t>& msg, bool encrypt)->void
   {
      BIP151Connection* connPtr = nullptr;
      if (encrypt)
         connPtr = ccs.bip151Connection_.get();
      SerializedMessage aeadMsg;
      aeadMsg.construct(msg, connPtr, type);
      ccs.serializedStack_->push_back(move(aeadMsg));
      ccs.count_->fetch_add(1, memory_order_relaxed);

      //call write callback
      lws_callback_on_writable(ccs.wsiPtr_);
   };

   auto processHandshake = [id, &writeToClient](
      const BinaryData& msgdata, ClientConnectionState& clientState)->bool
   {
      mutex mu;
      WebSocketMessagePartial wsMsg;
      
      if (!wsMsg.parsePacket(msgdata.getRef()) || !wsMsg.isReady())
      {
         //invalid packet
         return false;
      }

      auto dataBdr = wsMsg.getSingleBinaryMessage();
      switch (wsMsg.getType())
      {
      case WS_MSGTYPE_AEAD_SETUP:
      {
         unique_lock<mutex> lock(mu);
         //init bip151 handshake
         vector<uint8_t> encinitData(ENCINITMSGSIZE);
         if (clientState.bip151Connection_->getEncinitData(
               &encinitData[0], ENCINITMSGSIZE,
               BIP151SymCiphers::CHACHA20POLY1305_OPENSSH) != 0)
         {
            //failed to init handshake, kill connection
            return false;
         }

         writeToClient(clientState, WS_MSGTYPE_AEAD_ENCINIT, encinitData, false);
         break;
      }

      case WS_MSGTYPE_AEAD_ENCACK:
      {
         //process client encack
         if (clientState.bip151Connection_->processEncack(
            dataBdr.getPtr(), dataBdr.getSize(), true) != 0)
         {
            //failed to init handshake, kill connection
            return false;
         }

         break;
      }

      case WS_MSGTYPE_AEAD_REKEY:
      {
         if (clientState.bip151Connection_->getBIP150State() !=
            BIP150State::SUCCESS)
         {
            //can't rekey before auth, kill connection
            return false;
         }

         //process rekey
         if (clientState.bip151Connection_->processEncack(
            dataBdr.getPtr(), dataBdr.getSize(), false) != 0)
         {
            //failed to init handshake, kill connection
            LOGWARN << "failed to process rekey";
            return false;
         }

         break;
      }

      case WS_MSGTYPE_AEAD_ENCINIT:
      {
         unique_lock<mutex> lock(mu);
         //process client encinit
         if (clientState.bip151Connection_->processEncinit(
            dataBdr.getPtr(), dataBdr.getSize(), false) != 0)
         {
            //failed to init handshake, kill connection
            return false;
         }

         //return encack
         vector<uint8_t> encackData(BIP151PUBKEYSIZE);
         if (clientState.bip151Connection_->getEncackData(
            &encackData[0], BIP151PUBKEYSIZE) != 0)
         {
            //failed to init handshake, kill connection
            return false;
         }

         writeToClient(
            clientState, WS_MSGTYPE_AEAD_ENCACK, encackData, false);

         break;
      }

      case WS_MSGTYPE_AUTH_CHALLENGE:
      {
         bool goodChallenge = true;
         auto challengeResult =
            clientState.bip151Connection_->processAuthchallenge(
               dataBdr.getPtr(),
               dataBdr.getSize(),
               true); //true: step #1 of 6

         if(challengeResult == -1)
         {
            //auth fail, kill connection
            return false;
         }
         else if (challengeResult == 1)
         {
            goodChallenge = false;
         }

         BinaryData authreplyBuf(BIP151PRVKEYSIZE * 2);
         if (clientState.bip151Connection_->getAuthreplyData(
            authreplyBuf.getPtr(),
            authreplyBuf.getSize(),
            true, //true: step #2 of 6
            goodChallenge) == -1)
         {
            //auth setup failure, kill connection
            return false;
         }

         writeToClient(
            clientState, WS_MSGTYPE_AUTH_REPLY,
            authreplyBuf.getDataVector(), true);

         break;
      }

      case WS_MSGTYPE_AUTH_PROPOSE:
      {
         bool goodPropose = true;
         auto proposeResult = clientState.bip151Connection_->processAuthpropose(
            dataBdr.getPtr(),
            dataBdr.getSize());

         if(proposeResult == -1)
         {
            //auth setup failure, kill connection
            return false;
         }
         else if (proposeResult == 1)
         {
            goodPropose = false;
         }
         else
         {
            //keep track of the propose check state
            clientState.bip151Connection_->setGoodPropose();
         }

         BinaryData authchallengeBuf(BIP151PRVKEYSIZE);
         if(clientState.bip151Connection_->getAuthchallengeData(
            authchallengeBuf.getPtr(),
            authchallengeBuf.getSize(),
            "", //empty string, use chosen key from processing auth propose
            false, //false: step #4 of 6
            goodPropose) == -1)
         { 
            //auth setup failure, kill connection
            return false;
         }

         writeToClient(
            clientState, WS_MSGTYPE_AUTH_CHALLENGE,
            authchallengeBuf.getDataVector(), true);

         break;
      }

      case WS_MSGTYPE_AUTH_REPLY:
      {
         if (clientState.bip151Connection_->processAuthreply(
            dataBdr.getPtr(),
            dataBdr.getSize(),
            false,
            clientState.bip151Connection_->getProposeFlag()) != 0)
         {
            //invalid auth setup, kill connection
            return false;
         }

         //rekey after succesful BIP150 handshake
         clientState.bip151Connection_->bip150HandshakeRekey();
         clientState.outKeyTimePoint_ = chrono::system_clock::now();

         break;
      }
         
      default:
         //unexpected msg id, kill connection
         return false;
      }

      return true;
   };

   auto processAEAD = [this, id, &processHandshake](BinaryData msgdata)->void
   {
      auto clientStateMap = getConnectionStateMap();
      auto iter = clientStateMap->find(id);
      if (iter == clientStateMap->end())
      {
         //invalid client id, return
         return;
      }

      if (!processHandshake(msgdata, iter->second))
         closeClientConnection(id);
   };

   if (async)
   {
      thread thr(processAEAD, move(msg));
      if (thr.joinable())
         thr.detach();

      return;
   }

   processAEAD(move(msg));
}

///////////////////////////////////////////////////////////////////////////////
AuthPeersLambdas WebSocketServer::getAuthPeerLambda(void) const
{
   auto authPeerPtr = authorizedPeers_;

   auto getMap = [authPeerPtr](void)->const map<string, btc_pubkey>&
   {
      return authPeerPtr->getPeerNameMap();
   };

   auto getPrivKey = [authPeerPtr](
      const BinaryDataRef& pubkey)->const SecureBinaryData&
   {
      return authPeerPtr->getPrivateKey(pubkey);
   };

   auto getAuthSet = [authPeerPtr](void)->const set<SecureBinaryData>&
   {
      return authPeerPtr->getPublicKeySet();
   };

   return AuthPeersLambdas(getMap, getPrivKey, getAuthSet);
}

///////////////////////////////////////////////////////////////////////////////
void WebSocketServer::closeClientConnection(uint64_t id)
{
   auto clientStateMap = getConnectionStateMap();
   auto iter = clientStateMap->find(id);
   if (iter == clientStateMap->end())
   {
      //invalid client id, return
      return;
   }

   iter->second.closeFlag_->store(-1, memory_order_relaxed);
}
