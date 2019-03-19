#include <chrono>
#include <QStandardPaths>
#include "zmq.h"

#include "FastLock.h"
#include "MessageHolder.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "ZMQ_BIP15X_Msg.h"

using namespace std;

// The constructor to use.
//
// INPUT:  Logger object. (const shared_ptr<spdlog::logger>&)
//         Server info. (const ArmoryServersProvider&) (REMOVE?)
//         Ephemeral peer usage. Not recommended. (const bool&)
// OUTPUT: None
zmqBIP15XDataConnection::zmqBIP15XDataConnection(
   const shared_ptr<spdlog::logger>& logger
   , const ArmoryServersProvider& trustedServer, const bool& ephemeralPeers
   , bool monitored)
   : ZmqDataConnection(logger, monitored) {
   string datadir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
   string filename(CLIENT_AUTH_PEER_FILENAME);

   // In general, load the server key from a special Armory wallet file.
   if (!ephemeralPeers) {
      authPeers_ = make_shared<AuthorizedPeers>(
         datadir, filename);
   }
   else {
      authPeers_ = make_shared<AuthorizedPeers>();
   }

   // BIP 151 connection setup. Technically should be per-socket or something
   // similar but data connections will only connect to one machine at a time.
   auto lbds = getAuthPeerLambda();
   bip151Connection_ = make_shared<BIP151Connection>(lbds);
}

// Get lambda functions related to authorized peers. Copied from Armory.
//
// INPUT:  None
// OUTPUT: None
// RETURN: AuthPeersLambdas object with required lambdas.
AuthPeersLambdas zmqBIP15XDataConnection::getAuthPeerLambda() const
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

// The send function for the data connection. Ideally, this should not be used
// before the handshake is completed, but it is possible to use at any time.
// Whether or not the raw data is used, it will be placed in a zmqBIP15XMsg
// object.
//
// INPUT:  The data to send. It'll be encrypted here if needed. (const string&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XDataConnection::send(const string& data) {
   string message = data;

   // Check if we need to do a rekey before sending the data.
   bool needs_rekey = false;
   auto rightnow = chrono::system_clock::now();

   // For now, disable time-based key cycling. This should be re-enabled
   // eventually.
/*   if (bip151Connection_->rekeyNeeded(message->getSerializedSize())) {
      needs_rekey = true;
   }
   else {
      auto time_sec = chrono::duration_cast<chrono::seconds>(
         rightnow - outKeyTimePoint_);
      if (time_sec.count() >= AEAD_REKEY_INVERVAL_SECONDS)
         needs_rekey = true;
   }

   if (needs_rekey) {
      BinaryData rekeyData(BIP151PUBKEYSIZE);
      memset(rekeyData.getPtr(), 0, BIP151PUBKEYSIZE);

      zmqBIP15XMsg rekeyPacket;
      std::vector<BinaryData> outPacket = rekeyPacket.serialize(rekeyData.getRef()
         , bip151Connection_.get(), ZMQ_MSGTYPE_AEAD_REKEY, 0);

      if (!send(move(outPacket[0].toBinStr()))) {
    std::cout << "DEBUG: zmqBIP15XDataConnection::send - Rekey send failed" << dataLen << std::endl;
      }
      bip151Connection_->rekeyOuterSession();
      outKeyTimePoint_ = rightnow;
      ++outerRekeyCount_;
   }*/

   // Encrypt data here only after the BIP 150 handshake is complete.
   string sendData = data;
   size_t dataLen = sendData.size();
   if (bip151Connection_->getBIP150State() == BIP150State::SUCCESS) {
      zmqBIP15XMsg msg;
      BIP151Connection* connPtr = nullptr;
      if (bip151HandshakeCompleted_) {
         connPtr = bip151Connection_.get();
      }
      BinaryData payload(data);
      vector<BinaryData> encData = msg.serialize(payload.getDataVector()
         , connPtr, ZMQ_MSGTYPE_SINGLEPACKET, 0);
      sendData = encData[0].toBinStr();
      dataLen = sendData.size();
   }

   int result = -1;
   {
      FastLock locker(lockSocket_);
      result = zmq_send(dataSocket_.get(), sendData.c_str(), dataLen, 0);
   }
   if (result != (int)data.size()) {
      if (logger_) {
         logger_->error("[zmqBIP15XDataConnection::{}] {} failed to send "
            "data: {}", __func__, connectionName_, zmq_strerror(zmq_errno()));
      }
      return false;
   }

   return true;
}

// Kick off the BIP 151 handshake. This is the first function to call once the
// unencrypted connection is established.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XDataConnection::startBIP151Handshake() {
   zmqBIP15XMsg msg;
   BinaryData nullPayload;

   vector<BinaryData> outData = msg.serialize(nullPayload.getDataVector()
      , nullptr, ZMQ_MSGTYPE_AEAD_SETUP, 0);
   return send(move(outData[0].toBinStr()));
}

// The function that handles raw data coming in from the socket. The data may or
// may not be encrypted.
//
// INPUT:  The raw incoming data. It may or may not be encrypted. (const string&)
// OUTPUT: None
// RETURN: None
void zmqBIP15XDataConnection::onRawDataReceived(const string& rawData) {
   // Place the data in the processing queue and process the queue.
   pendingData_.append(rawData);
   ProcessIncomingData();
}

// Close the connection.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XDataConnection::closeConnection() {
//   bip151Connection_.reset();
   return ZmqDataConnection::closeConnection();
}

// The function that processes raw ZMQ connection data. It processes the BIP
// 150/151 handshake (if necessary) and decrypts the raw data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void zmqBIP15XDataConnection::ProcessIncomingData() {
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
         if (logger_) {
            logger_->error("[zmqBIP15XDataConnection::{}] Decryption failed "
               "(connection {}) - error code {}", __func__, connectionName_
               , result);
         }
         return;
      }

      payload.resize(payload.getSize() - POLY1305MACLEN);
   }

   // Deserialize the packet.
   zmqBIP15XMsg inMsg;
   if (!inMsg.parsePacket(payload.getRef())) {
      if (logger_) {
         logger_->error("[zmqBIP15XDataConnection::{}] Packet parsing failed "
            "(connection {})", __func__, connectionName_);
      }
      return;
   }

   // If the BIP 150/151 handshake isn't complete, take the next handshake step.
   if (inMsg.getType() > ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      if (!processAEADHandshake(inMsg)) {
         if (logger_) {
            logger_->error("[zmqBIP15XDataConnection::{}] Encryption "
               "handshake failed (connection {})", __func__, connectionName_);
         }
         return;
      }

      return;
   }

   // We shouldn't get here but just in case....
   if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
      if (logger_) {
         logger_->error("[zmqBIP15XDataConnection::{}] Encryption handshake "
            "is incomplete (connection {})", __func__, connectionName_);
      }
      return;
   }

   // Figure out request ID and fulfill promise. Ignored for now.
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

      logger_->error("[{}] No callbacks for now.", __func__);
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

   // Pass the final data up the chain.
   auto&& outMsg = inMsg.getSingleBinaryMessage();
   if (outMsg.getSize() == 0) {
      logger_->error("[{}] - Incoming packet is empty", __func__);
      return;
   }
   ZmqDataConnection::notifyOnData(outMsg.toBinStr());
}

// Create the data socket.
//
// INPUT:  None
// OUTPUT: None
// RETURN: The data socket. (ZmqContext::sock_ptr)
ZmqContext::sock_ptr zmqBIP15XDataConnection::CreateDataSocket() {
   return context_->CreateClientSocket();
}

// Get the incoming data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XDataConnection::recvData() {
   MessageHolder data;

   int result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      if (logger_) {
         logger_->error("[zmqBIP15XDataConnection::{}] {} failed to recv data "
            "frame from stream: {}" , __func__, connectionName_
            , zmq_strerror(zmq_errno()));
      }
      return false;
   }

   // Process the raw data.
   onRawDataReceived(data.ToString());
   return true;
}

// The function processing the BIP 150/151 handshake packets.
//
// INPUT:  The handshake packet. (const zmqBIP15XMsg&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XDataConnection::processAEADHandshake(
   const zmqBIP15XMsg& msgObj) {
   // Function used to send data out on the wire.
   auto writeData = [this](BinaryData& payload, uint8_t type, bool encrypt) {
      zmqBIP15XMsg msg;
      BIP151Connection* connPtr = nullptr;
      if (encrypt) {
         connPtr = bip151Connection_.get();
      }

      vector<BinaryData> outData = msg.serialize(payload.getDataVector()
         , connPtr, type, 0);
      send(move(outData[0].toBinStr()));
   };

   // Read the message, get the type, and process as needed. Code mostly copied
   // from Armory.
   auto msgbdr = msgObj.getSingleBinaryMessage();
   switch (msgObj.getType()) {
   case ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY:
   {
      /*packet is server's pubkey, do we have it?*/

      //init server promise
      serverPubkeyProm_ = make_shared<promise<bool>>();

      //compute server name
      stringstream ss;
      ss << hostAddr_ << ":" << hostPort_;

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
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AEAD_ENCINIT not processed");
         return false;
      }

      //valid encinit, send client side encack
      BinaryData encackPayload(BIP151PUBKEYSIZE);
      if (bip151Connection_->getEncackData(encackPayload.getPtr()
         , BIP151PUBKEYSIZE) != 0) {
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AEAD_ENCACK data not obtained");
         return false;
      }

      writeData(encackPayload, ZMQ_MSGTYPE_AEAD_ENCACK, false);

      //start client side encinit
      BinaryData encinitPayload(ENCINITMSGSIZE);
      if (bip151Connection_->getEncinitData(encinitPayload.getPtr()
         , ENCINITMSGSIZE, BIP151SymCiphers::CHACHA20POLY1305_OPENSSH) != 0) {
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AEAD_ENCINIT data not obtained");
         return false;
      }

      writeData(encinitPayload, ZMQ_MSGTYPE_AEAD_ENCINIT, false);

      break;
   }

   case ZMQ_MSGTYPE_AEAD_ENCACK:
   {
      if (bip151Connection_->processEncack(msgbdr.getPtr(), msgbdr.getSize()
         , true) == -1) {
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AEAD_ENCACK not processed");
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
      stringstream ss;
      ss << hostAddr_ << ":" << hostPort_;

      BinaryData authchallengeBuf(BIP151PRVKEYSIZE);
      if (bip151Connection_->getAuthchallengeData(
         authchallengeBuf.getPtr(), authchallengeBuf.getSize(), ss.str()
         , true //true: auth challenge step #1 of 6
         , false) != 0) { //false: have not processed an auth propose yet
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AUTH_CHALLENGE data not obtained");
         return false;
      }

      writeData(authchallengeBuf, ZMQ_MSGTYPE_AUTH_CHALLENGE, true);
      bip151HandshakeCompleted_ = true;
      break;
   }

   case ZMQ_MSGTYPE_AEAD_REKEY:
   {
      // Rekey requests before auth are invalid.
      if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - Not ready to rekey");
         return false;
      }

      // If connection is already setup, we only accept rekey enack messages.
      if (bip151Connection_->processEncack(msgbdr.getPtr(), msgbdr.getSize()
         , false) == -1) {
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AEAD_REKEY not processed");
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
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AUTH_REPLY not processed");
         return false;
      }

      BinaryData authproposeBuf(BIP151PRVKEYSIZE);
      if (bip151Connection_->getAuthproposeData(
         authproposeBuf.getPtr(),
         authproposeBuf.getSize()) != 0) {
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AUTH_PROPOSE data not obtained");
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
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AUTH_CHALLENGE not processed");
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

      if (validReply != 0) {
         //auth setup failure, kill connection
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - AUTH_REPLY data not obtained");
         return false;
      }

      writeData(authreplyBuf, ZMQ_MSGTYPE_AUTH_REPLY, true);

      //rekey
      bip151Connection_->bip150HandshakeRekey();
      outKeyTimePoint_ = chrono::system_clock::now();
      emit bip15XCompleted();

      break;
   }

   default:
      logger_->error("[processHandshake] Unknown message type.");
      return false;
   }

   return true;
}

// If the user is presented with a new server identity key, ask what they want.
void zmqBIP15XDataConnection::promptUser(const BinaryDataRef& newKey
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
