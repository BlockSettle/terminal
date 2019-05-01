#include <chrono>
#include <QStandardPaths>

#include "FastLock.h"
#include "MessageHolder.h"
#include "EncryptionUtils.h"
#include "SystemFileUtils.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "BIP150_151.h"

using namespace std;

#define HEARTBEAT_PACKET_SIZE 23

// The constructor to use.
//
// INPUT:  Logger object. (const shared_ptr<spdlog::logger>&)
//         Ephemeral peer usage. Not recommended. (const bool&)
//         A flag for a monitored socket. (const bool&)
// OUTPUT: None
ZmqBIP15XDataConnection::ZmqBIP15XDataConnection(
   const shared_ptr<spdlog::logger>& logger, const bool& ephemeralPeers
   , const bool& monitored, const bool& makeClientCookie
   , const bool& readServerCookie, const std::string& cookieNamePath)
   : ZmqDataConnection(logger, monitored), makeClientIDCookie_(makeClientCookie)
   , useServerIDCookie_(readServerCookie), bipIDCookiePath_(cookieNamePath)
{
   if (makeClientIDCookie_ && useServerIDCookie_) {
      throw std::runtime_error("Cannot read client ID cookie and create ID " \
         "cookie at the same time. Connection is incomplete.");
   }

   if (makeClientIDCookie_ && bipIDCookiePath_.empty()) {
      throw std::runtime_error("ID cookie creation requested but no name " \
         "supplied. Connection is incomplete.");
   }

   if (useServerIDCookie_ && bipIDCookiePath_.empty()) {
      throw std::runtime_error("ID cookie reading requested but no name " \
         "supplied. Connection is incomplete.");
   }

   outKeyTimePoint_ = chrono::steady_clock::now();
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
   if (makeClientIDCookie_) {
      genBIPIDCookie();
   }

   const auto &heartbeatProc = [this] {
      while (hbThreadRunning_) {
         {
            std::unique_lock<std::mutex> lock(hbMutex_);
            hbCondVar_.wait_for(lock, std::chrono::seconds{ 1 });
            if (!hbThreadRunning_) {
               break;
            }
         }
         if (!bip151Connection_ || (bip151Connection_->getBIP150State() != BIP150State::SUCCESS)) {
            continue;
         }
         const auto curTime = std::chrono::steady_clock::now();
         const auto diff =
            std::chrono::duration_cast<std::chrono::milliseconds>(
            curTime - lastHeartbeat_);
         if (diff.count() > heartbeatInterval_) {
            triggerHeartbeat();
         }
      }
   };
   hbThreadRunning_ = true;
   lastHeartbeat_ = std::chrono::steady_clock::now();
   hbThread_ = std::thread(heartbeatProc);
}

ZmqBIP15XDataConnection::~ZmqBIP15XDataConnection()
{
   hbThreadRunning_ = false;
   hbCondVar_.notify_one();
   hbThread_.join();

   // If it exists, delete the identity cookie.
   if (makeClientIDCookie_) {
//      const string absCookiePath =
//         SystemFilePaths::appDataLocation() + "/" + bipIDCookieName_;
      if (SystemFileUtils::fileExist(bipIDCookiePath_)) {
         if (!SystemFileUtils::rmFile(bipIDCookiePath_)) {
            logger_->error("[{}] Unable to delete client identity cookie ({})."
               , __func__, bipIDCookiePath_);
         }
      }
   }
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

// A function that handles any required rekeys before data is sent.
//
// ***This function must be called before any data is sent on the wire.***
//
// INPUT:  N/A
// OUTPUT: N/A
// RETURN: N/A
void ZmqBIP15XDataConnection::rekeyIfNeeded(const size_t& dataSize)
{
   bool needsRekey = false;
   const auto rightNow = chrono::steady_clock::now();

   if (bip150HandshakeCompleted_)
   {
      // Rekey off # of bytes sent or length of time since last rekey.
      if (bip151Connection_->rekeyNeeded(dataSize))
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
         if (!sendPacket(packet.toBinStr()))
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
}

// An internal send function to be used when this class constructs a packet. The
// packet must already be encrypted (if required) and ready to be sent.
//
// ***Please use this function for sending all data from inside this class.***
//
// INPUT:  The data to send. (const string&)
//         Flag for encryption usage. True by default. (const bool&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::sendPacket(const string& data)
{
   int result = -1;
   const auto rightNow = chrono::steady_clock::now();

   {
      FastLock locker(lockSocket_);
      result = zmq_send(dataSocket_.get(), data.c_str(), data.size(), 0);
   }
   if (result != (int)data.size()) {
      if (logger_) {
         logger_->error("[ZmqBIP15XDataConnection::{}] {} failed to send "
            "data: {} (result={}, data size={})", __func__, connectionName_
            , zmq_strerror(zmq_errno()), result, data.size());
      }
      return false;
   }

   lastHeartbeat_ = rightNow; // Reset the heartbeat timer, as data was sent.
   return true;
}

// The inherited send function for the data connection. It is intended to be
// used only for raw data that has not yet been encrypted or placed in a packet.
//
// ***Please use this function for sending all data from outside this class.***
//
// INPUT:  The data to send. (const string&)
//         Flag for encryption usage. True by default. (const bool&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::send(const string& data)
{
   bool retVal = false;

   // If we need to rekey, do it before encrypting the data.
   rekeyIfNeeded(data.size());

   // Encrypt data here only after the BIP 150 handshake is complete, and if
   // the incoming encryption flag is true.
   string sendData = data;
   if (bip151Connection_->getBIP150State() == BIP150State::SUCCESS) {
      ZmqBIP15XSerializedMessage msg;
      BIP151Connection* connPtr = nullptr;
      if (bip151HandshakeCompleted_) {
         connPtr = bip151Connection_.get();
      }

      BinaryData payload(data);
      msg.construct(payload.getDataVector(), connPtr
         , ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER, msgID_);

      // Cycle through all packets.
      while (!msg.isDone())
      {
         auto& packet = msg.getNextPacket();
         if (packet.getSize() == 0) {
            logger_->error("[ZmqBIP15XClientConnection::{}] failed to "
               "serialize data (size {})", __func__, data.size());
            return retVal;
         }

         retVal = sendPacket(packet.toBinStr());
         if (!retVal)
         {
            logger_->error("[ZmqBIP15XServerConnection::{}] fragment send failed"
               , __func__);
            return retVal;
         }
      }
   }

   return retVal;
}

void ZmqBIP15XDataConnection::notifyOnConnected()
{
   startBIP151Handshake([this] {
      ZmqDataConnection::notifyOnConnected();
   });
}

// A function that is used to trigger heartbeats. Required because ZMQ is unable
// to tell, via a data socket connection, when a client has disconnected.
//
// INPUT:  N/A
// OUTPUT: N/A
// RETURN: N/A
void ZmqBIP15XDataConnection::triggerHeartbeat()
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

   // If a rekey is needed, rekey before encrypting. Estimate the size of the
   // final packet first in order to get the # of bytes transmitted.
   rekeyIfNeeded(HEARTBEAT_PACKET_SIZE);

   ZmqBIP15XSerializedMessage msg;
   BinaryData emptyPayload;
   msg.construct(emptyPayload.getDataVector(), connPtr, ZMQ_MSGTYPE_HEARTBEAT
      , msgID_);
   auto& packet = msg.getNextPacket();

   // An error message is already logged elsewhere if the send fails.
   if (!sendPacket(packet.toBinStr()), false) {  // sendPacket already sets the timestamp
      notifyOnDisconnected();
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
   return sendPacket(packet.toBinStr());
}

// The function that handles raw data coming in from the socket. The data may or
// may not be encrypted.
//
// INPUT:  The raw incoming data. (const string&)
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
   // If a future obj is still waiting, satisfy it to prevent lockup. This
   // shouldn't happen here but it's an emergency fallback.
   if (serverPubkeyProm_ && !serverPubkeySignalled_) {
      serverPubkeyProm_->set_value(false);
      serverPubkeySignalled_ = true;
   }
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
   if (!result) {
      if (logger_) {
         logger_->error("[ZmqBIP15XDataConnection::{}] Deserialization failed "
            "(connection {})", __func__, connectionName_);
      }

      currentReadMessage_.reset();
      notifyOnError(DataConnectionListener::SerializationFailed);
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
      if (!processAEADHandshake(currentReadMessage_.message_)) {
         if (logger_) {
            logger_->error("[ZmqBIP15XDataConnection::{}] Handshake failed "
               "(connection {})", __func__, connectionName_);
         }

         notifyOnError(DataConnectionListener::HandshakeFailed);
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
      notifyOnError(DataConnectionListener::HandshakeFailed);
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

   currentReadMessage_.reset();
   // Pass the final data up the chain.
   ZmqDataConnection::notifyOnData(inMsg.toBinStr());
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
      sendPacket(packet.toBinStr());
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

      // If it's a local connection, get a cookie with the server's key.
      if (useServerIDCookie_) {
         // Read the cookie with the key to check.
         BinaryData cookieKey(static_cast<size_t>(BTC_ECKEY_COMPRESSED_LENGTH));
         if (!getServerIDCookie(cookieKey)) {
            return false;
         }
         else {
            // Add the host and the key to the list of verified peers. Be sure
            // to erase any old keys first.
            vector<string> keyName;
            string localAddrV4 = hostAddr_ + ":" + hostPort_;
            keyName.push_back(localAddrV4);
            authPeers_->eraseName(localAddrV4);
            authPeers_->addPeer(cookieKey, keyName);
         }
      }

      //compute server name
      stringstream ss;
      ss << hostAddr_ << ":" << hostPort_;

      // If we don't have the key already, we may ask the the user if they wish
      // to continue. (Remote signer only.)
      if (!bip151Connection_->havePublicKey(msgbdr, ss.str())) {
         //we don't have this key, call user prompt lambda
         verifyNewIDKey(msgbdr, ss.str());
      }
      else {
         //set server key promise
         if (serverPubkeyProm_ && !serverPubkeySignalled_) {
            serverPubkeyProm_->set_value(true);
            serverPubkeySignalled_ = true;
         }
         else {
            logger_->warn("[processHandshake] server public key was already set");
         }
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

      // Do we need to check the server's ID key?
      if (serverPubkeyProm_ != nullptr) {
         //if so, wait on the promise
         auto fut = serverPubkeyProm_->get_future();
         fut.wait();

         if (fut.get()) {
            serverPubkeyProm_.reset();
            serverPubkeySignalled_ = false;
         }
         else {
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCACK - Server public key not verified");
            return false;
         }
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
      outKeyTimePoint_ = chrono::steady_clock::now();
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

// Set the callback to be used when asking if the user wishes to accept BIP 150
// identity keys from a server. See the design notes in the header for details.
//
// INPUT:  The callback that will ask the user to confirm the new key. (std::function)
// OUTPUT: N/A
// RETURN: N/A
void ZmqBIP15XDataConnection::setCBs(const cbNewKey& inNewKeyCB) {
   if (makeClientIDCookie_) {
      logger_->error("[{}] Cannot use callbacks when using cookies.", __func__);
      return;
   }

   // Set callbacks only if callbacks actually exist.
   if (inNewKeyCB) {
      cbNewKey_ = inNewKeyCB;
   }
}

// Add an authorized peer's BIP 150 identity key manually.
//
// INPUT:  The authorized key. (const BinaryData)
//         The server IP address (or host name) and port. (const string)
// OUTPUT: N/A
// RETURN: N/A
void ZmqBIP15XDataConnection::addAuthPeer(const BinaryData& inKey
   , const std::string& keyName)
{
   if (!(CryptoECDSA().VerifyPublicKeyValid(inKey))) {
      logger_->error("[{}] BIP 150 authorized key ({}) for user {} is invalid."
         , __func__,  inKey.toHexStr(), keyName);
      return;
   }
   authPeers_->eraseName(keyName);
   authPeers_->addPeer(inKey, vector<string>{ keyName });
}

// If the user is presented with a new remote server ID key it doesn't already
// know about, verify the key. A promise will also be used in case any functions
// are waiting on verification results.
//
// INPUT:  The key to verify. (const BinaryDataRef)
//         The server IP address (or host name) and port. (const string)
// OUTPUT: N/A
// RETURN: N/A
void ZmqBIP15XDataConnection::verifyNewIDKey(const BinaryDataRef& newKey
   , const string& srvAddrPort)
{
   if (useServerIDCookie_) {
      // If we get here, it's because the cookie add failed or the cookie was
      // incorrect. Satisfy the promise to prevent lockup.
      logger_->error("[{}] Server ID key cookie could not be verified", __func__);
      if (serverPubkeyProm_ && !serverPubkeySignalled_) {
         serverPubkeyProm_->set_value(false);
         serverPubkeySignalled_ = true;
      }
      notifyOnError(DataConnectionListener::HandshakeFailed);
      return;
   }

   // Check to see if we've already verified the key. If not, fire the
   // callback allowing the user to verify the new key.
   auto authPeerNameMap = authPeers_->getPeerNameMap();
   auto authPeerNameSearch = authPeerNameMap.find(srvAddrPort);
   if (authPeerNameSearch == authPeerNameMap.end()) {
      logger_->info("[{}] New key ({}) for server [{}] arrived.", __func__
         , newKey.toHexStr(), srvAddrPort);

      if (!cbNewKey_) {
         logger_->error("[{}] no server key callback is set - aborting handshake", __func__);
         notifyOnError(DataConnectionListener::HandshakeFailed);
         return;
      }

      // Ask the user if they wish to accept the new identity key.
      BinaryData oldKey; // there shouldn't be any old key, at least in authPeerNameSearch
      cbNewKey_(oldKey.toHexStr(), newKey.toHexStr(), serverPubkeyProm_);
      serverPubkeySignalled_ = true;

      //have we seen the server's pubkey?
      if (serverPubkeyProm_ != nullptr) {
         //if so, wait on the promise
         auto fut = serverPubkeyProm_->get_future();
         fut.wait();
         serverPubkeyProm_.reset();
         serverPubkeySignalled_ = false;
      }

      // Add the key. Old keys aren't deleted automatically. Do it to be safe.
      vector<string> keyName;
      keyName.push_back(srvAddrPort);
      authPeers_->eraseName(srvAddrPort);
      authPeers_->addPeer(newKey.copy(), keyName);
      logger_->info("[{}] Server at {} has had its identity key (0x{}) "
         "replaced with (0x{}). Connection accepted.", __func__, srvAddrPort
         , oldKey.toHexStr(), newKey.toHexStr());
   }
}

// Get the signer's identity public key. Intended for use with local signers.
//
// INPUT:  The accompanying key IP:Port or name. (const string)
// OUTPUT: The buffer that will hold the compressed ID key. (BinaryData)
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::getServerIDCookie(BinaryData& cookieBuf)
{
   if (!useServerIDCookie_) {
      return false;
   }

   if (!SystemFileUtils::fileExist(bipIDCookiePath_)) {
      logger_->error("[{}] Server identity cookie ({}) doesn't exist. Unable "
         "to verify server identity.", __func__, bipIDCookiePath_);
      return false;
   }

   // Ensure that we only read a compressed key.
   ifstream cookieFile(bipIDCookiePath_, ios::in | ios::binary);
   cookieFile.read(cookieBuf.getCharPtr(), BIP151PUBKEYSIZE);
   cookieFile.close();
   if (!(CryptoECDSA().VerifyPublicKeyValid(cookieBuf))) {
      logger_->error("[{}] Server identity key ({}) isn't a valid compressed "
         "key. Unable to verify server identity.", __func__
         , cookieBuf.toHexStr());
      return false;
   }

   return true;
}

// Generate a cookie with the client's identity public key.
//
// INPUT:  N/A
// OUTPUT: N/A
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::genBIPIDCookie()
{
   if (!makeClientIDCookie_) {
      logger_->error("[{}] ID cookie creation requested but not allowed."
      , __func__);
      return false;
   }

   if (SystemFileUtils::fileExist(bipIDCookiePath_)) {
      if (!SystemFileUtils::rmFile(bipIDCookiePath_)) {
         logger_->error("[{}] Unable to delete client identity cookie ({}). "
            "Will not write a new cookie.", __func__, bipIDCookiePath_);
         return false;
      }
   }

   // Ensure that we only write the compressed key.
   ofstream cookieFile(bipIDCookiePath_, ios::out | ios::binary);
   const BinaryData ourIDKey = getOwnPubKey();
   if (ourIDKey.getSize() != BTC_ECKEY_COMPRESSED_LENGTH) {
      logger_->error("[{}] Client identity key ({}) is uncompressed. Will not "
         "write the identity cookie.", __func__, bipIDCookiePath_);
      return false;
   }

   logger_->debug("[{}] Writing a new client identity cookie ({}).", __func__
      ,  bipIDCookiePath_);
   cookieFile.write(getOwnPubKey().getCharPtr(), BTC_ECKEY_COMPRESSED_LENGTH);
   cookieFile.close();

   return true;
}

// Get the client's compressed BIP 150 identity public key.
//
// INPUT:  N/A
// OUTPUT: N/A
// RETURN: A buffer with the compressed ECDSA ID pub key. (BinaryData)
BinaryData ZmqBIP15XDataConnection::getOwnPubKey() const
{
   const auto pubKey = authPeers_->getOwnPublicKey();
   return BinaryData(pubKey.pubkey, pubKey.compressed
      ? BTC_ECKEY_COMPRESSED_LENGTH : BTC_ECKEY_UNCOMPRESSED_LENGTH);
}
