#include <chrono>
#include <QStandardPaths>

#include "FastLock.h"
#include "MessageHolder.h"
#include "ZMQ_BIP15X_DataConnection.h"

using namespace std;

// Reset a partial message.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void ZmqBIP15XMsgFragments::reset(void)
{
   packets_.clear();
   message_.reset();
}

// Insert data into a partial message.
//
// INPUT:  The data to insert. (BinaryData&)
// OUTPUT: None
// RETURN: A reference to the packet data. (BinaryDataRef)
BinaryDataRef ZmqBIP15XMsgFragments::insertDataAndGetRef(BinaryData& data)
{
   auto&& data_pair = std::make_pair(counter_++, std::move(data));
   auto iter = packets_.insert(std::move(data_pair));
   return iter.first->second.getRef();
}

// Erase the last packet in a partial message.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void ZmqBIP15XMsgFragments::eraseLast(void)
{
   if (counter_ == 0)
      return;

   packets_.erase(counter_--);
}

// The constructor to use.
//
// INPUT:  Logger object. (const shared_ptr<spdlog::logger>&)
//         Ephemeral peer usage. Not recommended. (const bool&)
// OUTPUT: None
ZmqBIP15XDataConnection::ZmqBIP15XDataConnection(
   const shared_ptr<spdlog::logger>& logger
   , const bool& ephemeralPeers, bool monitored)
   : ZmqDataConnection(logger, monitored)
{
   outKeyTimePoint_ = chrono::system_clock::now();
   currentReadMessage_.reset();
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

   // Create a random four-byte ID for the client.
   msgID_ = READ_UINT32_LE(CryptoPRNG::generateRandom(4));

   // BIP 151 connection setup. Technically should be per-socket or something
   // similar but data connections will only connect to one machine at a time.
   auto lbds = getAuthPeerLambda();
   bip151Connection_ = make_shared<BIP151Connection>(lbds);

   const auto &heartbeatProc = [this] {
      while (hbThreadRunning_) {
         {
            std::unique_lock<std::mutex> lock(hbMutex_);
            hbCondVar_.wait_for(lock, std::chrono::seconds{ 1 });
            if (!hbThreadRunning_) {
               break;
            }
         }
         const auto curTime = std::chrono::system_clock::now();
         const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(curTime - lastHeartbeat_);
         if (diff.count() > heartbeatInterval_) {
            sendHeartbeat();
         }
      }
   };
   hbThreadRunning_ = true;
   lastHeartbeat_ = std::chrono::system_clock::now();
   hbThread_ = std::thread(heartbeatProc);
}

ZmqBIP15XDataConnection::~ZmqBIP15XDataConnection()
{
   hbThreadRunning_ = false;
   hbCondVar_.notify_one();
   hbThread_.join();
}

// Get lambda functions related to authorized peers. Copied from Armory.
//
// INPUT:  None
// OUTPUT: None
// RETURN: AuthPeersLambdas object with required lambdas.
AuthPeersLambdas ZmqBIP15XDataConnection::getAuthPeerLambda() const
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
// Whether or not the raw data is used, it will be placed in a
// ZmqBIP15XSerializedMessage object.
//
// INPUT:  The data to send. It'll be encrypted here if needed. (const string&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::send(const string& data)
{

   // Check if we need to do a rekey before sending the data.
   bool needsRekey = false;
   const auto rightNow = chrono::system_clock::now();

   if (bip150HandshakeCompleted_)
   {
      // Rekey off # of bytes sent or length of time since last rekey.
      if (bip151Connection_->rekeyNeeded(data.size()))
      {
         needsRekey = true;
      }
      else
      {
         auto time_sec = chrono::duration_cast<chrono::seconds>(
            rightNow - outKeyTimePoint_);
         if (time_sec.count() >= ZMQ_AEAD_REKEY_INVERVAL_SECS)
         {
            needsRekey = true;
         }
      }

      if (needsRekey)
      {
         outKeyTimePoint_ = rightNow;
         BinaryData rekeyData(BIP151PUBKEYSIZE);
         memset(rekeyData.getPtr(), 0, BIP151PUBKEYSIZE);

         ZmqBIP15XSerializedMessage rekeyPacket;
         rekeyPacket.construct(rekeyData.getRef(), bip151Connection_.get()
            , ZMQ_MSGTYPE_AEAD_REKEY);

         auto& packet = rekeyPacket.getNextPacket();
         if (!send(packet.toBinStr()))
         {
            if (logger_) {
               logger_->error("[ZmqBIP15XDataConnection::{}] {} failed to send "
                  "rekey: {} (result={})", __func__, connectionName_
                  , zmq_strerror(zmq_errno()));
            }
         }
         bip151Connection_->rekeyOuterSession();
         ++outerRekeyCount_;
      }
   }

   // Encrypt data here only after the BIP 150 handshake is complete.
   string sendData = data;
   size_t dataLen = sendData.size();
   if (bip151Connection_->getBIP150State() == BIP150State::SUCCESS) {
      ZmqBIP15XSerializedMessage msg;
      BIP151Connection* connPtr = nullptr;
      if (bip151HandshakeCompleted_) {
         connPtr = bip151Connection_.get();
      }
      BinaryData payload(data);
      msg.construct(payload.getDataVector(), connPtr
         , ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER, msgID_);
      sendData = msg.getNextPacket().toBinStr();
      dataLen = sendData.size();
   }

   // Now, we can send the data.
   int result = -1;
   {
      FastLock locker(lockSocket_);
      result = zmq_send(dataSocket_.get(), sendData.c_str(), dataLen, 0);
   }
   if (result != (int)dataLen) {
      if (logger_) {
         logger_->error("[ZmqBIP15XDataConnection::{}] {} failed to send "
            "data: {} (result={}, data size={})", __func__, connectionName_
            , zmq_strerror(zmq_errno()), result, dataLen);
      }
      return false;
   }

   lastHeartbeat_ = rightNow;    // Each other message sent resets the heartbeat timer
   return true;
}

void ZmqBIP15XDataConnection::sendHeartbeat()
{
   if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
      logger_->error("[ZmqBIP15XDataConnection::{}] {} invalid state: {}"
         , __func__, connectionName_, (int)bip151Connection_->getBIP150State());
      return;
   }
   BIP151Connection* connPtr = nullptr;
   if (bip151HandshakeCompleted_) {
      connPtr = bip151Connection_.get();
   }

   uint32_t size = 1;
   BinaryData plainText(4 + size + POLY1305MACLEN);

   // Copy in packet size.
   memcpy(plainText.getPtr(), &size, 4);
   size += 4;
   //type
   plainText.getPtr()[4] = ZMQ_MSGTYPE_HEARTBEAT;

   //encrypt if possible
   if (connPtr != nullptr) {
      connPtr->assemblePacket(plainText.getPtr(), size, plainText.getPtr()
         , size + POLY1305MACLEN);
   } else {
      plainText.resize(size);
   }

   int result = -1;
   {
      FastLock locker(lockSocket_);
      result = zmq_send(dataSocket_.get(), plainText.getCharPtr()
         , plainText.getSize(), 0);
   }
   if (result != (int)plainText.getSize()) {
      logger_->error("[ZmqBIP15XDataConnection::{}] {} failed to send "
         "data: {} (result={}, data size={}", __func__, connectionName_
         , zmq_strerror(zmq_errno()), result, plainText.getSize());
   }
   else {
      lastHeartbeat_ = chrono::system_clock::now();
   }
}

// Kick off the BIP 151 handshake. This is the first function to call once the
// unencrypted connection is established.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::startBIP151Handshake(
   const std::function<void()> &cbCompleted)
{
   ZmqBIP15XSerializedMessage msg;
   cbCompleted_ = cbCompleted;
   BinaryData nullPayload;

   msg.construct(nullPayload.getDataVector(), nullptr, ZMQ_MSGTYPE_AEAD_SETUP,
      0);
   auto& packet = msg.getNextPacket();
   return send(packet.toBinStr());
}

// The function that handles raw data coming in from the socket. The data may or
// may not be encrypted.
//
// INPUT:  The raw incoming data. It may or may not be encrypted. (const string&)
// OUTPUT: None
// RETURN: None
void ZmqBIP15XDataConnection::onRawDataReceived(const string& rawData)
{
   BinaryData payload(rawData);

   // If decryption "failed" due to fragmentation, put the pieces together.
   // (Unlikely but we need to plan for it.)
   if (leftOverData_.getSize() != 0)
   {
      leftOverData_.append(payload);
      payload = move(leftOverData_);
      leftOverData_.clear();
   }

   // Perform decryption if we're ready.
   if (bip151Connection_->connectionComplete())
   {
      auto result = bip151Connection_->decryptPacket(
         payload.getPtr(), payload.getSize(),
         payload.getPtr(), payload.getSize());

      // Failure isn't necessarily a problem if we're dealing with fragments.
      if (result != 0)
      {
         // If decryption "fails" but the result indicates fragmentation, save
         // the fragment and wait before doing anything, otherwise treat it as a
         // legit error.
         if (result <= ZMQ_MESSAGE_PACKET_SIZE && result > -1)
         {
            leftOverData_ = move(payload);
            return;
         }
         else
         {
            logger_->error("[{}] Packet decryption failed - Error {}", __func__
               , result);
            return;
         }
      }

      payload.resize(payload.getSize() - POLY1305MACLEN);
   }

   ProcessIncomingData(payload);
}

// Close the connection.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::closeConnection()
{
   currentReadMessage_.reset();
   return ZmqDataConnection::closeConnection();
}

// The function that processes raw ZMQ connection data. It processes the BIP
// 150/151 handshake (if necessary) and decrypts the raw data.
//
// INPUT:  Reformed message. (const BinaryData&) // TO DO: FIX UP MSG
// OUTPUT: None
// RETURN: None
void ZmqBIP15XDataConnection::ProcessIncomingData(BinaryData& payload)
{
   // Deserialize packet.
   auto payloadRef = currentReadMessage_.insertDataAndGetRef(payload);
   auto result = currentReadMessage_.message_.parsePacket(payloadRef);
   if (!result)
   {
      if (logger_) {
         logger_->error("[ZmqBIP15XDataConnection::{}] Deserialization failed "
            "(connection {})", __func__, connectionName_);
      }

      currentReadMessage_.reset();
      return;
   }

   // Fragmented messages may not be marked as fragmented when decrypted but may
   // still be a fragment. That's fine. Just wait for the other fragments.
   if (!currentReadMessage_.message_.isReady())
   {
      return;
   }

   // If we're still handshaking, take the next step. (No fragments allowed.)
   if (currentReadMessage_.message_.getType() > ZMQ_MSGTYPE_AEAD_THRESHOLD)
   {
      if (!processAEADHandshake(currentReadMessage_.message_))
      {
         if (logger_) {
            logger_->error("[ZmqBIP15XDataConnection::{}] Handshake failed "
               "(connection {})", __func__, connectionName_);
         }
         return;
      }

      currentReadMessage_.reset();
      return;
   }

   // We can now safely obtain the full message.
   BinaryData inMsg;
   currentReadMessage_.message_.getMessage(&inMsg);

   // We shouldn't get here but just in case....
   if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
      if (logger_) {
         logger_->error("[ZmqBIP15XDataConnection::{}] Encryption handshake "
            "is incomplete (connection {})", __func__, connectionName_);
      }
      return;
   }

   // For now, ignore the BIP message ID. If we need callbacks later, we can go
   // back to what's in Armory and add support based off that.
/*   auto& msgid = currentReadMessage_.message_.getId();
   switch (msgid)
   {
   case ZMQ_CALLBACK_ID:
   {
      break;
   }

   default:
      break;
   }*/

   // Pass the final data up the chain.
   ZmqDataConnection::notifyOnData(inMsg.toBinStr());
   currentReadMessage_.reset();
}

// Create the data socket.
//
// INPUT:  None
// OUTPUT: None
// RETURN: The data socket. (ZmqContext::sock_ptr)
ZmqContext::sock_ptr ZmqBIP15XDataConnection::CreateDataSocket()
{
   return context_->CreateClientSocket();
}

// Get the incoming data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::recvData()
{
   MessageHolder data;

   int result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      if (logger_) {
         logger_->error("[ZmqBIP15XDataConnection::{}] {} failed to recv data "
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
// INPUT:  The handshake packet. (const ZmqBIP15XMsgPartial&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::processAEADHandshake(
   const ZmqBIP15XMsgPartial& msgObj)
{
   // Function used to send data out on the wire.
   auto writeData = [this](BinaryData& payload, uint8_t type, bool encrypt) {
      ZmqBIP15XSerializedMessage msg;
      BIP151Connection* connPtr = nullptr;
      if (encrypt) {
         connPtr = bip151Connection_.get();
      }

      msg.construct(payload.getDataVector(), connPtr, type, 0);
      auto& packet = msg.getNextPacket();
      send(packet.toBinStr());
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

      // Rekey.
      bip151Connection_->bip150HandshakeRekey();
      bip150HandshakeCompleted_ = true;
      outKeyTimePoint_ = chrono::system_clock::now();
      emit bip15XCompleted();
      if (cbCompleted_) {
         cbCompleted_();
      }

      break;
   }

   default:
      logger_->error("[processHandshake] Unknown message type.");
      return false;
   }

   return true;
}

// If the user is presented with a new server identity key, ask what they want.
void ZmqBIP15XDataConnection::promptUser(const BinaryDataRef& newKey
   , const string& srvAddrPort)
{
   // TO DO: Insert a user prompt. For now, just approve the key and add it to
   // the set of approved keys. This needs to be fixed ASAP!
   auto authPeerNameMap = authPeers_->getPeerNameMap();
   auto authPeerNameSearch = authPeerNameMap.find(srvAddrPort);
   if (authPeerNameSearch == authPeerNameMap.end()) {
      logger_->info("[{}] New key ({}) for server [{}] arrived.", __func__
         , newKey.toHexStr(), srvAddrPort);
      vector<string> keyName;
      keyName.push_back(srvAddrPort);
      authPeers_->addPeer(newKey.copy(), keyName);
      serverPubkeyProm_->set_value(true);
   }
   else {
      serverPubkeyProm_->set_value(true);
   }
}

SecureBinaryData ZmqBIP15XDataConnection::getOwnPubKey() const
{
   const auto pubKey = authPeers_->getOwnPublicKey();
   return SecureBinaryData(pubKey.pubkey, pubKey.compressed
      ? BTC_ECKEY_COMPRESSED_LENGTH : BTC_ECKEY_UNCOMPRESSED_LENGTH);
}
