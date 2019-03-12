#include <chrono>
#include <QStandardPaths>
#include "zmq.h"

#include "FastLock.h"
#include "MessageHolder.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "ZMQ_BIP15X_Msg.h"

using namespace std;

ZMQ_BIP15X_DataConnection::ZMQ_BIP15X_DataConnection(
   const shared_ptr<spdlog::logger>& logger
   , const ArmoryServersProvider& trustedServer, const bool& ephemeralPeers)
   : ZmqDataConnection(logger) {
   string datadir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
   string filename(CLIENT_AUTH_PEER_FILENAME);
   if (!ephemeralPeers) {
      authPeers_ = make_shared<AuthorizedPeers>(datadir, filename);
   }
   else {
      authPeers_ = make_shared<AuthorizedPeers>();
   }

   auto lbds = getAuthPeerLambda();
   bip151Connection_ = make_shared<BIP151Connection>(lbds);
}

ZMQ_BIP15X_DataConnection::ZMQ_BIP15X_DataConnection(
   const shared_ptr<spdlog::logger>& logger
   , const ArmoryServersProvider& trustedServer, const bool& ephemeralPeers
   , bool monitored)
   : ZmqDataConnection(logger, monitored) {
   string datadir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
   string filename(CLIENT_AUTH_PEER_FILENAME);
   if (!ephemeralPeers) {
      authPeers_ = make_shared<AuthorizedPeers>(
         datadir, filename);
   }
   else {
      authPeers_ = make_shared<AuthorizedPeers>();
   }

   auto lbds = getAuthPeerLambda();
   bip151Connection_ = make_shared<BIP151Connection>(lbds);
}

// Copied from Armory.
AuthPeersLambdas ZMQ_BIP15X_DataConnection::getAuthPeerLambda() const
{
   auto authPeerPtr = authPeers_;

   auto getMap = [authPeerPtr](void)->const map<string, btc_pubkey>& {
      return authPeerPtr->getPeerNameMap();
   };

   auto getPrivKey = [authPeerPtr](
      const BinaryDataRef& pubkey)->const SecureBinaryData& {
      return authPeerPtr->getPrivateKey(pubkey);
   };

   auto getAuthSet = [authPeerPtr](void)->const set<SecureBinaryData>& {
      return authPeerPtr->getPublicKeySet();
   };

   return AuthPeersLambdas(getMap, getPrivKey, getAuthSet);
}

/////////////////////////////////////////////
bool ZMQ_BIP15X_DataConnection::send(const string& data) {
   string message = data;

// TO DO: CLEAN ALL THIS UP.
   //check for rekey
   {
      bool needs_rekey = false;
      auto rightnow = chrono::system_clock::now();

/*      if (bip151Connection_->rekeyNeeded(message->getSerializedSize()))
      {
         needs_rekey = true;
      }
      else
      {
         auto time_sec = chrono::duration_cast<chrono::seconds>(
            rightnow - outKeyTimePoint_);
         if (time_sec.count() >= AEAD_REKEY_INVERVAL_SECONDS)
            needs_rekey = true;
      }*/

/*      if (needs_rekey)
      {
         BinaryData rekeyPacket(BIP151PUBKEYSIZE);
         memset(rekeyPacket.getPtr(), 0, BIP151PUBKEYSIZE);

// TO DO: FIX ID VALUE, FIX
         std::vector<BinaryData> outPacket = serialize(rekeyPacket.getRef()
            , bip151Connection_.get(), ZMQ_MSGTYPE_AEAD_REKEY, 0);
         SerializedMessage rekey_msg;
         rekey_msg.construct(
            rekeyPacket.getDataVector(),
            bip151Connection_.get(), ZMQ_MSGTYPE_AEAD_REKEY);

         if (!ZmqDataConnection::sendRawData(message)) {
            // TO DO: FIX THIS
         }
         bip151Connection_->rekeyOuterSession();
         outKeyTimePoint_ = rightnow;
         ++outerRekeyCount_;
      }*/
   }

   // TO DO: Generate output data
   // Generate outgoing packet.
/*   SerializedMessage ws_msg;
   ws_msg.construct(
      data, bip151Connection_.get(),
      ZMQ_MSGTYPE_SINGLEPACKET, message->id_);

   writeQueue_.push_back(move(ws_msg));*/

   int result = -1;
   {
      FastLock locker(lockSocket_);
      result = zmq_send(dataSocket_.get(), data.c_str(), data.size(), 0);
   }
   if (result != (int)data.size()) {
      if (logger_) {
         logger_->error("[ZMQ_BIP15X_DataConnection::{}] {} failed to send "
            "data: {}", __func__, connectionName_, zmq_strerror(zmq_errno()));
      }
      return false;
   }

   return true;
}

bool ZMQ_BIP15X_DataConnection::startBIP151Handshake() {
   ZMQ_BIP15X_Msg msg;
   BIP151Connection* connPtr = nullptr;
   BinaryData nullPayload;

   vector<BinaryData> outData = msg.serialize(nullPayload.getDataVector()
      , connPtr, ZMQ_MSGTYPE_AEAD_SETUP, 0);
   send(move(outData[0].toBinStr()));
}

// The function that handles raw data coming in from a ZMQ socket.
void ZMQ_BIP15X_DataConnection::onRawDataReceived(const string& rawData) {
   pendingData_.append(rawData);
   ProcessIncomingData();
}

// The function that processes raw ZMQ connection data. It processes the BIP
// 150/151 handshake (if necessary) and decrypts the raw data.
void ZMQ_BIP15X_DataConnection::ProcessIncomingData() {
   size_t position;

   // Process all incoming data while clearing the buffer.
   BinaryData payload(pendingData_);
   pendingData_.clear();

   // If we've completed the BIP 151 handshake, decrypt.
   if (bip151HandshakeCompleted_) {
      //decrypt packet
      auto result = bip151Connection_->decryptPacket(
         payload.getPtr(), payload.getSize(),
         payload.getPtr(), payload.getSize());

      if (result != 0) {
/*         //see WebSocketServer::commandThread for the explainantion
         if (result <= WEBSOCKET_MESSAGE_PACKET_SIZE && result > -1)
         {
            leftOverData_ = move(payload);
            continue;
         }*/

         return;
      }

      payload.resize(payload.getSize() - POLY1305MACLEN);
   }

   //deser packet
   ZMQ_BIP15X_Msg inMsg;
   if (!inMsg.parsePacket(payload.getRef())) {
      cout << "DEBUG: Parse packet error" << endl;
      return;
   }

   if (inMsg.getType() > ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      if (!processAEADHandshake(inMsg)) {
         //invalid AEAD message, kill connection
         return;
      }

//      continue;
      return;
   }

   if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
      cout << "DEBUG: encryption layer is uninitialized, aborting connection" << endl;
      return;
   }

   //figure out request id, fulfill promise
   auto& msgid = inMsg.getId();
   switch (msgid) {
   case ZMQ_CALLBACK_ID:
   {
/*      if (callbackPtr_ == nullptr)
      {
         continue;
      }

      auto msgptr = make_shared<::Codec_BDVCommand::BDVCallback>();
      if (!currentReadMessage_.message_.getMessage(msgptr.get()))
      {
         currentReadMessage_.reset();
         continue;
      }

      callbackPtr_->processNotifications(msgptr);
      currentReadMessage_.reset();*/

      cout << "DEBUG: No callbacks for now." << endl;
      break;
   }

   default:
      // Used for callbacks. Not worrying about it for now.
/*      auto readMap = readPackets_.get();
      auto iter = readMap->find(msgid);
      if (iter != readMap->end())
      {
         auto& msgObjPtr = iter->second;
         auto callbackPtr = dynamic_cast<CallbackReturn_WebSocket*>(
            msgObjPtr->payload_->callbackReturn_.get());
         if (callbackPtr == nullptr)
            continue;

         callbackPtr->callback(currentReadMessage_.message_);
         readPackets_.erase(msgid);
         currentReadMessage_.reset();
      }
      else
      {
         LOGWARN << "invalid msg id";
         currentReadMessage_.reset();
      }*/
      break;
   }

   ZmqDataConnection::notifyOnData(payload.toBinStr());
}

ZmqContext::sock_ptr ZMQ_BIP15X_DataConnection::CreateDataSocket() {
   return context_->CreateClientSocket();
}

bool ZMQ_BIP15X_DataConnection::recvData() {
   MessageHolder data;

   int result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      if (logger_) {
         logger_->error("[ZMQ_BIP15X_DataConnection::{}] {} failed to recv data "
            "frame from stream: {}" , __func__, connectionName_
            , zmq_strerror(zmq_errno()));
      }
      return false;
   }

   onRawDataReceived(data.ToString());

   return true;
}

// The function processing the BIP 150/151 handshake packets.
bool ZMQ_BIP15X_DataConnection::processAEADHandshake(
   const ZMQ_BIP15X_Msg& msgObj) {
   // Callback used to send data out on the wire.
   auto writeData = [this](BinaryData& payload, uint8_t type, bool encrypt) {
      ZMQ_BIP15X_Msg msg;
      BIP151Connection* connPtr = nullptr;
      if (encrypt) {
         connPtr = bip151Connection_.get();
      }

      vector<BinaryData> outData = msg.serialize(payload.getDataVector()
         , connPtr, type, 0);
      send(move(outData[0].toBinStr()));
   };

   // Read the msg, get the type, and process as needed.
   auto msgbdr = msgObj.getSingleBinaryMessage();
   switch (msgObj.getType()) {
   case ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY:
   {
      /*packet is server's pubkey, do we have it?*/

      //init server promise
      serverPubkeyProm_ = make_shared<promise<bool>>();

      //compute server name
// TO DO: Get server addr & port into this funct.
      stringstream ss;
//      ss << addr_ << ":" << port_;

      if (!bip151Connection_->havePublicKey(msgbdr, ss.str())) {
         //we don't have this key, call user prompt lambda
         promptUser(msgbdr, ss.str());
      }
      else {
         //set server key promise
         serverPubkeyProm_->set_value(true);
      }

      break;
   }

   case ZMQ_MSGTYPE_AEAD_ENCINIT:
   {
      if (bip151Connection_->processEncinit(msgbdr.getPtr(), msgbdr.getSize()
         , false) != 0) {
         return false;
      }

      //valid encinit, send client side encack
      BinaryData encackPayload(BIP151PUBKEYSIZE);
      if (bip151Connection_->getEncackData(encackPayload.getPtr()
         , BIP151PUBKEYSIZE) != 0) {
         return false;
      }

      writeData(encackPayload, ZMQ_MSGTYPE_AEAD_ENCACK, false);

      //start client side encinit
      BinaryData encinitPayload(ENCINITMSGSIZE);
      if (bip151Connection_->getEncinitData(encinitPayload.getPtr()
         , ENCINITMSGSIZE, BIP151SymCiphers::CHACHA20POLY1305_OPENSSH) != 0) {
         return false;
      }

      writeData(encinitPayload, ZMQ_MSGTYPE_AEAD_ENCINIT, false);

      break;
   }

   case ZMQ_MSGTYPE_AEAD_ENCACK:
   {
      if (bip151Connection_->processEncack(msgbdr.getPtr(), msgbdr.getSize()
         , true) == -1) {
         return false;
      }

      //have we seen the server's pubkey?
      if (serverPubkeyProm_ != nullptr) {
         //if so, wait on the promise
         auto serverProm = serverPubkeyProm_;
         auto fut = serverProm->get_future();
         fut.wait();
         serverPubkeyProm_.reset();
      }

      //bip151 handshake completed, time for bip150
// TO DO: Get server addr & port into this funct.
      stringstream ss;
//      ss << addr_ << ":" << port_;

      BinaryData authchallengeBuf(BIP151PRVKEYSIZE);
      if (bip151Connection_->getAuthchallengeData(
         authchallengeBuf.getPtr(), authchallengeBuf.getSize(), ss.str()
         , true //true: auth challenge step #1 of 6
         , false) != 0) { //false: have not processed an auth propose yet
         return false;
      }

      writeData(authchallengeBuf, ZMQ_MSGTYPE_AUTH_CHALLENGE, true);
      bip151HandshakeCompleted_ = true;
      break;
   }

   case ZMQ_MSGTYPE_AEAD_REKEY:
   {
      //rekey requests before auth are invalid
      if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
         return false;
      }

      //if connection is already setup, we only accept rekey enack messages
      if (bip151Connection_->processEncack(msgbdr.getPtr(), msgbdr.getSize()
         , false) == -1) {
         return false;
      }

      ++innerRekeyCount_;
      break;
   }

   case ZMQ_MSGTYPE_AUTH_REPLY:
   {
      if (bip151Connection_->processAuthreply(msgbdr.getPtr(), msgbdr.getSize()
         , true //true: step #2 out of 6
         , false) != 0) { //false: haven't seen an auth challenge yet
         return false;
      }

      BinaryData authproposeBuf(BIP151PRVKEYSIZE);
      if (bip151Connection_->getAuthproposeData(
         authproposeBuf.getPtr(),
         authproposeBuf.getSize()) != 0) {
         return false;
      }

      writeData(authproposeBuf, ZMQ_MSGTYPE_AUTH_PROPOSE, true);
      break;
   }

   case ZMQ_MSGTYPE_AUTH_CHALLENGE:
   {
      bool goodChallenge = true;
      auto challengeResult =
         bip151Connection_->processAuthchallenge(msgbdr.getPtr()
            , msgbdr.getSize(), false); //true: step #4 of 6

      if (challengeResult == -1) {
         //auth fail, kill connection
         return false;
      }
      else if (challengeResult == 1) {
         goodChallenge = false;
      }

      BinaryData authreplyBuf(BIP151PRVKEYSIZE * 2);
      auto validReply = bip151Connection_->getAuthreplyData(
         authreplyBuf.getPtr(), authreplyBuf.getSize()
         , false //true: step #5 of 6
         , goodChallenge);

      writeData(authreplyBuf, ZMQ_MSGTYPE_AUTH_REPLY, true);

      if (validReply != 0) {
         //auth setup failure, kill connection
         return false;
      }

      //rekey
      bip151Connection_->bip150HandshakeRekey();
      outKeyTimePoint_ = chrono::system_clock::now();

      //flag connection as ready
//      connectionReadyProm_.set_value(true);

      break;
   }

   default:
      return false;
   }

   return true;
}

// If the user is presented with a new server identity key, ask what they want.
void ZMQ_BIP15X_DataConnection::promptUser(const BinaryDataRef& newKey
   , const string& srvAddrPort) {
   // TO DO: Insert a user prompt. For now, just approve the key and add it to
   // the set of approved key.
   auto authPeerNameMap = authPeers_->getPeerNameMap();
   auto authPeerNameSearch = authPeerNameMap.find(srvAddrPort);
   if (authPeerNameSearch == authPeerNameMap.end()) {
      std::cout << "New key arrived. Prompt the user." << std::endl;
      vector<string> keyName;
      keyName.push_back(srvAddrPort);
      authPeers_->addPeer(newKey.copy(), keyName);
      serverPubkeyProm_->set_value(true);
   }
   else {
      serverPubkeyProm_->set_value(true);
   }
}
