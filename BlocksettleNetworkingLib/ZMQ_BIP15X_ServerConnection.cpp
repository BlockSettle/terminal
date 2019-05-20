#include "ZMQ_BIP15X_ServerConnection.h"

#include "FastLock.h"
#include "MessageHolder.h"
#include "SystemFileUtils.h"

#include <chrono>

using namespace std;

// A call resetting the encryption-related data for individual connections.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void ZmqBIP15XPerConnData::reset()
{
   encData_.reset();
   bip150HandshakeCompleted_ = false;
   bip151HandshakeCompleted_ = false;
   currentReadMessage_.reset();
   outKeyTimePoint_ = chrono::steady_clock::now();
}

// The constructor to use.
//
// INPUT:  Logger object. (const shared_ptr<spdlog::logger>&)
//         ZMQ context. (const std::shared_ptr<ZmqContext>&)
//         Per-connection ID. (const uint64_t&)
//         Callback for getting a list of trusted clients. (function<vector<string>()>)
//         Ephemeral peer usage. Not recommended. (const bool&)
//         The directory containing the file with the non-ephemeral key. (const std::string)
//         The file with the non-ephemeral key. (const std::string)
//         A flag indicating if the connection will make a key cookie. (bool)
//         A flag indicating if the connection will read a key cookie. (bool)
//         The path to the key cookie to read or write. (const std::string)
// OUTPUT: None
ZmqBIP15XServerConnection::ZmqBIP15XServerConnection(
   const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ZmqContext>& context
   , const uint64_t& id
   , const std::function<std::vector<std::string>()>& cbTrustedClients
   , const bool& ephemeralPeers, const std::string& ownKeyFileDir
   , const std::string& ownKeyFileName, const bool& makeServerCookie
   , const bool& readClientCookie, const std::string& cookiePath)
   : ZmqServerConnection(logger, context), id_(id)
   , useClientIDCookie_(readClientCookie)
   , makeServerIDCookie_(makeServerCookie)
   , bipIDCookiePath_(cookiePath)
   , cbTrustedClients_(cbTrustedClients)
   , heartbeatInterval_(getDefaultHeartbeatInterval())
{
   if (!ephemeralPeers && (ownKeyFileDir.empty() || ownKeyFileName.empty())) {
      throw std::runtime_error("Client requested static ID key but no key " \
         "wallet file is specified.");
   }

   if (makeServerIDCookie_ && readClientCookie) {
      throw std::runtime_error("Cannot read client ID cookie and create ID " \
         "cookie at the same time. Connection is incomplete.");
   }

   if (makeServerIDCookie_ && bipIDCookiePath_.empty()) {
      throw std::runtime_error("ID cookie creation requested but no name " \
         "supplied. Connection is incomplete.");
   }

   if (readClientCookie && bipIDCookiePath_.empty()) {
      throw std::runtime_error("ID cookie reading requested but no name " \
         "supplied. Connection is incomplete.");
   }

   // In general, load the client key from a special Armory wallet file.
   if (!ephemeralPeers) {
       authPeers_ = make_shared<AuthorizedPeers>(ownKeyFileDir, ownKeyFileName);
   }
   else {
      authPeers_ = make_shared<AuthorizedPeers>();
   }

   if (makeServerIDCookie_) {
      genBIPIDCookie();
   }
   heartbeatThread();
}

// A specialized server connection constructor with limited options. Used only
// for connections with ephemeral keys that use one-way verification (i.e.,
// clients aren't verified).
//
// INPUT:  Logger object. (const shared_ptr<spdlog::logger>&)
//         ZMQ context. (const std::shared_ptr<ZmqContext>&)
//         Callback for getting a list of trusted clients. (function<vector<string>()>)
//         A flag indicating if the connection will make a key cookie. (bool)
//         A flag indicating if the connection will read a key cookie. (bool)
//         The path to the key cookie to read or write. (const std::string)
// OUTPUT: None
ZmqBIP15XServerConnection::ZmqBIP15XServerConnection(
   const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ZmqContext>& context
   , const std::function<std::vector<std::string>()> &cbTrustedClients
   , const std::string& ownKeyFileDir, const std::string& ownKeyFileName
   , const bool& makeServerCookie, const bool& readClientCookie
   , const std::string& cookiePath)
   : ZmqServerConnection(logger, context)
   , cbTrustedClients_(cbTrustedClients), makeServerIDCookie_(makeServerCookie)
   , useClientIDCookie_(readClientCookie), bipIDCookiePath_(cookiePath)
{
   if (makeServerIDCookie_ && readClientCookie) {
      throw std::runtime_error("Cannot read client ID cookie and create ID " \
         "cookie at the same time. Connection is incomplete.");
   }

   if (makeServerIDCookie_ && bipIDCookiePath_.empty()) {
      throw std::runtime_error("ID cookie creation requested but no name " \
         "supplied. Connection is incomplete.");
   }

   if (readClientCookie && bipIDCookiePath_.empty()) {
      throw std::runtime_error("ID cookie reading requested but no name " \
         "supplied. Connection is incomplete.");
   }

   if (!ownKeyFileDir.empty() && !ownKeyFileName.empty()) {
      logger_->debug("[{}] creating/reading static key in {}/{}", __func__
         , ownKeyFileDir, ownKeyFileName);
      authPeers_ = make_shared<AuthorizedPeers>(ownKeyFileDir, ownKeyFileName);
   }
   else {
      logger_->debug("[{}] creating ephemeral key", __func__);
      authPeers_ = make_shared<AuthorizedPeers>();
   }
   BinaryData bdID = CryptoPRNG::generateRandom(8);
   id_ = READ_UINT64_LE(bdID.getPtr());

   if (makeServerIDCookie_) {
      genBIPIDCookie();
   }

   heartbeatThread();
}

ZmqBIP15XServerConnection::~ZmqBIP15XServerConnection()
{
   hbThreadRunning_ = false;
   hbCondVar_.notify_one();
   hbThread_.join();

   // If it exists, delete the identity cookie.
   if (makeServerIDCookie_) {
      if (SystemFileUtils::fileExist(bipIDCookiePath_)) {
         if (!SystemFileUtils::rmFile(bipIDCookiePath_)) {
            logger_->error("[{}] Unable to delete server identity cookie ({})."
               , __func__, bipIDCookiePath_);
         }
      }
   }
}

// The thread running the server heartbeat.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void ZmqBIP15XServerConnection::heartbeatThread()
{
   const auto &heartbeatProc = [this] {
      while (hbThreadRunning_) {
         {
            std::unique_lock<std::mutex> lock(hbMutex_);
            hbCondVar_.wait_for(lock, std::chrono::seconds{ 1 });
            if (!hbThreadRunning_) {
               break;
            }
         }
         const auto curTime = std::chrono::steady_clock::now();
         std::vector<std::string> timedOutClients;

         {
            FastLock locker{heartbeatsLock_};

            for (const auto &hbTime : lastHeartbeats_) {
               const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(curTime - hbTime.second);
               if (diff > heartbeatInterval_ * 2) {
                  timedOutClients.push_back(hbTime.first);
               }
            }
         }

         for (const auto &client : timedOutClients) {
            logger_->debug("[ZmqBIP15XServerConnection] client {} timed out"
               , BinaryData(client).toHexStr());
            resetBIP151Connection(client);
            notifyListenerOnDisconnectedClient(client);
         }
      }
   };

   hbThreadRunning_ = true;
   hbThread_ = std::thread(heartbeatProc);
}

// Create the data socket.
//
// INPUT:  None
// OUTPUT: None
// RETURN: The data socket. (ZmqContext::sock_ptr)
ZmqContext::sock_ptr ZmqBIP15XServerConnection::CreateDataSocket()
{
   return context_->CreateServerSocket();
}

// Get the incoming data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XServerConnection::ReadFromDataSocket()
{
   MessageHolder clientId;
   MessageHolder data;

   // The client ID will be sent before the actual data.
   int result = zmq_msg_recv(&clientId, dataSocket_.get(), ZMQ_DONTWAIT);
   if ((result == -1) || !clientId.GetSize())
   {
      logger_->error("[{}] {} failed to recv header: {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   // Now, we can grab the incoming data, whih includes the message ID once the
   // handshake is complete.
   result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1)
   {
      logger_->error("[{}] {} failed to recv message data: {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   // ZMQ should send one chunk of data, and not further fragment the data.
   if (!data.IsLast())
   {
      logger_->error("[{}] {} broken protocol", __func__, connectionName_);
      return false;
   }

   // Process the incoming data.
   ProcessIncomingData(data.ToString(), clientId.ToString());

   return true;
}

// The send function for the data connection. Ideally, this should not be used
// before the handshake is completed, but it is possible to use at any time.
// Whether or not the raw data is used, it will be placed in a
// ZmqBIP15XSerializedMessage object.
//
// INPUT:  The ZMQ client ID. (const string&)
//         The data to send. (const string&)
//         A post-send callback. Optional. (const SendResultCb&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XServerConnection::SendDataToClient(const string& clientId
   , const string& data, const SendResultCb& cb)
{
   BIP151Connection* connPtr = nullptr;

   auto connection = GetConnection(clientId);
   if (connection == nullptr) {
      logger_->error("[ZmqBIP15XServerConnection::SendDataToClient] missing client connection {}"
         , BinaryData(clientId).toHexStr());
      return false;
   }

   if (connection->bip151HandshakeCompleted_) {
      connPtr = connection->encData_.get();
   }

   // Check if we need to do a rekey before sending the data.
   if (connection->bip150HandshakeCompleted_) {
      bool needsRekey = false;
      auto rightNow = chrono::steady_clock::now();

      // Rekey off # of bytes sent or length of time since last rekey.
      if (connPtr->rekeyNeeded(data.size())) {
         needsRekey = true;
      }
      else {
         auto time_sec = chrono::duration_cast<chrono::seconds>(
            rightNow - connection->outKeyTimePoint_);
         if (time_sec.count() >= ZMQ_AEAD_REKEY_INVERVAL_SECS) {
            needsRekey = true;
         }
      }

      if (needsRekey) {
         connection->outKeyTimePoint_ = rightNow;
         rekey(clientId);
      }
   }

   {
      std::unique_lock<std::mutex> lock(rekeyMutex_);
      if (rekeyStarted_.find(clientId) != rekeyStarted_.end()) {
         pendingData_[clientId].push_back({ data, cb });
         return true;
      }
   }

   // Encrypt data here if the BIP 150 handshake is complete.
   if (connection->encData_ && connection->encData_->getBIP150State() == BIP150State::SUCCESS) {
      const BinaryData payload(data);
      ZmqBIP15XSerializedMessage msg;
      msg.construct(payload.getDataVector(), connPtr
         , ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER, connection->msgID_);

      // Cycle through all packets.
      while (!msg.isDone()) {
         auto& packet = msg.getNextPacket();
         if (packet.getSize() == 0) {
            logger_->error("[ZmqBIP15XServerConnection::SendDataToClient] failed to "
               "serialize data (size {})", data.size());
            return false;
         }

         if (!QueueDataToSend(clientId, packet.toBinStr(), cb, false)) {
            logger_->error("[ZmqBIP15XServerConnection::SendDataToClient] fragment send failed"
               , data.size());
            return false;
         }
      }

      return true;
   }

   // Queue up the untouched data for straight transmission.
   return QueueDataToSend(clientId, data, cb, false);
}

void ZmqBIP15XServerConnection::rekey(const std::string &clientId)
{
   auto connection = GetConnection(clientId);
   if (connection == nullptr) {
      logger_->error("[ZmqBIP15XServerConnection::rekey] can't find connection for {}", BinaryData(clientId).toHexStr());
      return;
   }

   if (!connection->bip151HandshakeCompleted_) {
      logger_->error("[ZmqBIP15XServerConnection::rekey] can't rekey {} without BIP151"
         " handshaked completed", BinaryData(clientId).toHexStr());
      return;
   }

   {
      std::unique_lock<std::mutex> lock(rekeyMutex_);
      rekeyStarted_.insert(clientId);
   }
   const auto connPtr = connection->encData_.get();
   BinaryData rekeyData(BIP151PUBKEYSIZE);
   memset(rekeyData.getPtr(), 0, BIP151PUBKEYSIZE);

   ZmqBIP15XSerializedMessage rekeyPacket;
   rekeyPacket.construct(rekeyData.getRef(), connPtr, ZMQ_MSGTYPE_AEAD_REKEY);
   auto& packet = rekeyPacket.getNextPacket();

   const auto &cbSent = [this, connection] (const std::string &clientId, const std::string &, bool result)
   {
      if (!result) {
         logger_->error("[ZmqBIP15XServerConnection::rekey] failed to send rekey");
         return;
      }
      logger_->debug("[ZmqBIP15XServerConnection::rekey] rekeying session for {}"
         , BinaryData(clientId).toHexStr());
      connection->encData_->rekeyOuterSession();
      ++connection->outerRekeyCount_;

      {
         std::unique_lock<std::mutex> lock(rekeyMutex_);
         rekeyStarted_.erase(clientId);
      }

      const auto itData = pendingData_.find(clientId);
      if ((itData != pendingData_.end()) && !itData->second.empty()) {
         for (const auto &data : itData->second) {
            SendDataToClient(itData->first, std::get<0>(data), std::get<1>(data));
         }
         itData->second.clear();
      }
   };

   if (!QueueDataToSend(clientId, packet.toBinStr(), cbSent, false)) {
      logger_->error("[ZmqBIP15XServerConnection::{}] {} failed to send "
         "rekey: {} (result={})", __func__, connectionName_
         , zmq_strerror(zmq_errno()));
   }
}

void ZmqBIP15XServerConnection::setLocalHeartbeatInterval()
{
   heartbeatInterval_ = getLocalHeartbeatInterval();
}

// static
const chrono::milliseconds ZmqBIP15XServerConnection::getDefaultHeartbeatInterval()
{
   return std::chrono::seconds(30);
}

// static
const chrono::milliseconds ZmqBIP15XServerConnection::getLocalHeartbeatInterval()
{
   return std::chrono::seconds(3);
}

// A send function for the data connection that sends data to all clients,
// somewhat like multicasting.
//
// INPUT:  The data to send. (const string&)
//         A post-send callback. Optional. (const SendResultCb&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XServerConnection::SendDataToAllClients(const std::string& data, const SendResultCb &cb)
{
   unsigned int successCount = 0;

   decltype(socketConnMap_) connectionMapCopy;

   {
      FastLock locker{connectionsLock_};
      connectionMapCopy = socketConnMap_;
   }

   for (const auto &it : connectionMapCopy) {
      if (SendDataToClient(it.first, data, cb)) {
         successCount++;
      }
   }
   return (successCount == connectionMapCopy.size());
}

// The function that processes raw ZMQ connection data. It processes the BIP
// 150/151 handshake (if necessary) and decrypts the raw data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void ZmqBIP15XServerConnection::ProcessIncomingData(const string& encData
   , const string& clientID)
{
   const auto connData = setBIP151Connection(clientID);

   if (!connData) {
      logger_->error("[ZmqBIP15XServerConnection::ProcessIncomingData] failed to find connection data for client {}"
         , BinaryData(clientID).toHexStr());
      notifyListenerOnClientError(clientID, "missing connection data");
      return;
   }

   BinaryData payload(encData);

   // If decryption "failed" due to fragmentation, put the pieces together.
   // (Unlikely but we need to plan for it.)
   if (leftOverData_.getSize() != 0) {
      leftOverData_.append(payload);
      payload = move(leftOverData_);
      leftOverData_.clear();
   }

   // Decrypt only if the BIP 151 handshake is complete.
   if (connData->bip151HandshakeCompleted_) {
      //decrypt packet
      auto result = connData->encData_->decryptPacket(
         payload.getPtr(), payload.getSize(),
         payload.getPtr(), payload.getSize());

      // Failure isn't necessarily a problem if we're dealing with fragments.
      if (result != 0) {
         // If decryption "fails" but the result indicates fragmentation, save
         // the fragment and wait before doing anything, otherwise treat it as a
         // legit error.
         if (result <= ZMQ_MESSAGE_PACKET_SIZE && result > -1) {
            leftOverData_ = move(payload);
         }
         else {
            logger_->error("[ZmqBIP15XServerConnection::{}] Packet decryption failed - Error {}"
               , __func__, result);
            notifyListenerOnClientError(clientID, "packet decryption failed");
         }
         return;
      }

      payload.resize(payload.getSize() - POLY1305MACLEN);
   }

   // Deserialize packet.
   auto payloadRef = connData->currentReadMessage_.insertDataAndGetRef(payload);
   auto result = connData->currentReadMessage_.message_.parsePacket(payloadRef);
   if (!result) {
      if (logger_) {
         logger_->error("[ZmqBIP15XServerConnection::{}] Deserialization failed "
            "(connection {})", __func__, connectionName_);
      }
      connData->currentReadMessage_.reset();
      notifyListenerOnClientError(clientID, "deserialization failed");
      return;
   }

   // Fragmented messages may not be marked as fragmented when decrypted but may
   // still be a fragment. That's fine. Just wait for the other fragments.
   if (!connData->currentReadMessage_.message_.isReady()) {
      return;
   }

   connData->msgID_ = connData->currentReadMessage_.message_.getId();

   // If we're still handshaking, take the next step. (No fragments allowed.)
   if (connData->currentReadMessage_.message_.getType() == ZMQ_MSGTYPE_HEARTBEAT) {
      UpdateClientHeartbeatTimestamp(clientID);
      connData->currentReadMessage_.reset();

      ZmqBIP15XSerializedMessage heartbeatPacket;
      BinaryData emptyPayload;
      heartbeatPacket.construct(emptyPayload.getDataVector(), connData->encData_.get(), ZMQ_MSGTYPE_HEARTBEAT);
      auto& packet = heartbeatPacket.getNextPacket();
      QueueDataToSend(clientID, packet.toBinStr(), {}, false);
      return;
   }
   else if (connData->currentReadMessage_.message_.getType() >
      ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      if (!processAEADHandshake(connData->currentReadMessage_.message_, clientID)) {
         if (logger_) {
            logger_->error("[ZmqBIP15XServerConnection::{}] Handshake failed "
               "(connection {})", __func__, connectionName_);
         }
         notifyListenerOnClientError(clientID, "handshake failed");
         return;
      }

      connData->currentReadMessage_.reset();
      return;
   }

   // We can now safely obtain the full message.
   BinaryData outMsg;
   connData->currentReadMessage_.message_.getMessage(&outMsg);

   // We shouldn't get here but just in case....
   if (connData->encData_->getBIP150State() !=
      BIP150State::SUCCESS) {
      if (logger_) {
         logger_->error("[ZmqBIP15XServerConnection::{}] Encryption handshake "
            "is incomplete (connection {})", __func__, connectionName_);
      }
      notifyListenerOnClientError(clientID, "encryption handshake failure");
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
   notifyListenerOnData(clientID, outMsg.toBinStr());
   connData->currentReadMessage_.reset();
}

// The function processing the BIP 150/151 handshake packets.
//
// INPUT:  The raw handshake packet data. (const BinaryData&)
//         The client ID. (const string&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XServerConnection::processAEADHandshake(
   const ZmqBIP15XMsgPartial& msgObj, const string& clientID)
{
   // Function used to actually send data to the client.
   auto writeToClient = [this, clientID](uint8_t type, const BinaryDataRef& msg
      , bool encrypt)->bool
   {
      ZmqBIP15XSerializedMessage outMsg;
      BIP151Connection* connPtr = nullptr;
      if (encrypt) {
         auto connection = GetConnection(clientID);
         if (connection == nullptr) {
            logger_->error("[ZmqBIP15XServerConnection::processAEADHandshake::writeToClient] no connection for client {}"
               , BinaryData(clientID).toHexStr());
            return false;
         }
         connPtr = connection->encData_.get();
      }

      // Construct the message and fire it down the pipe.
      outMsg.construct(msg, connPtr, type, 0);
      auto& packet = outMsg.getNextPacket();
      return SendDataToClient(clientID, packet.toBinStr());
   };

   // Handshake function. Code mostly copied from Armory.
   auto processHandshake = [this, writeToClient, clientID, msgObj]()->bool
   {
      auto connection = GetConnection(clientID);
      if (connection == nullptr) {
         logger_->error("[ZmqBIP15XServerConnection::processAEADHandshake::processHandshake] no connection for client {}"
               , BinaryData(clientID).toHexStr());
            return false;
      }

      // Parse the packet.
      auto dataBdr = msgObj.getSingleBinaryMessage();
      switch (msgObj.getType())
      {
      case ZMQ_MSGTYPE_AEAD_SETUP:
      {
         // If it's a local connection, get a cookie with the client's key.
         if (useClientIDCookie_) {
            // Read the cookie with the key to check.
            BinaryData cookieKey(static_cast<size_t>(BTC_ECKEY_COMPRESSED_LENGTH));
            if (!getClientIDCookie(cookieKey)) {
               notifyListenerOnClientError(clientID, "missing client cookie");
               return false;
            }
            else {
               // Add the host and the key to the list of verified peers. Be sure
               // to erase any old keys first.
               vector<string> keyName;
               keyName.push_back(clientID);
               authPeers_->eraseName(clientID);
               authPeers_->addPeer(cookieKey, keyName);
            }
         }

         //send pubkey message
         if (!writeToClient(ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY,
            connection->encData_->getOwnPubKey(), false))
         {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AEAD_SETUP: "
               "Response 1 not sent");
         }

         //init bip151 handshake
         BinaryData encinitData(ENCINITMSGSIZE);
         if (connection->encData_->getEncinitData(
            encinitData.getPtr(), ENCINITMSGSIZE
            , BIP151SymCiphers::CHACHA20POLY1305_OPENSSH) != 0)
         {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCINIT data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AEAD_ENCINIT, encinitData.getRef()
            , false))
         {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AEAD_SETUP: "
               "Response 2 not sent");
         }
         break;
      }

      case ZMQ_MSGTYPE_AEAD_ENCACK:
      {
         //process client encack
         if (connection->encData_->processEncack(dataBdr.getPtr()
            , dataBdr.getSize(), true) != 0)
         {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCACK not processed");
            return false;
         }

         break;
      }

      case ZMQ_MSGTYPE_AEAD_REKEY:
      {
         // Rekey requests before auth are invalid
         if (connection->encData_->getBIP150State() !=
            BIP150State::SUCCESS)
         {
            //can't rekey before auth, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - Not yet able to process a rekey");
            return false;
         }

         // If connection is already set up, we only accept rekey encack messages.
         if (connection->encData_->processEncack(dataBdr.getPtr()
            , dataBdr.getSize(), false) != 0)
         {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCACK not processed");
            return false;
         }

         connection->innerRekeyCount_++;
         break;
      }

      case ZMQ_MSGTYPE_AEAD_ENCINIT:
      {
         //process client encinit
         if (connection->encData_->processEncinit(dataBdr.getPtr()
            , dataBdr.getSize(), false) != 0)
         {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCINIT processing failed");
            return false;
         }

         //return encack
         BinaryData encackData(BIP151PUBKEYSIZE);
         if (connection->encData_->getEncackData(encackData.getPtr()
            , BIP151PUBKEYSIZE) != 0)
         {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_ENCACK data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AEAD_ENCACK, encackData.getRef()
            , false))
         {
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCACK not sent");
         }

         connection->bip151HandshakeCompleted_ = true;
         break;
      }

      case ZMQ_MSGTYPE_AUTH_CHALLENGE:
      {
         bool goodChallenge = true;
         auto challengeResult =
            connection->encData_->processAuthchallenge(
            dataBdr.getPtr(), dataBdr.getSize(), true); //true: step #1 of 6

         if (challengeResult == -1)
         {
            //auth fail, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_CHALLENGE processing failed");
            return false;
         }
         else if (challengeResult == 1)
         {
            goodChallenge = false;
         }

         BinaryData authreplyBuf(BIP151PRVKEYSIZE * 2);
         if (connection->encData_->getAuthreplyData(
            authreplyBuf.getPtr(), authreplyBuf.getSize()
            , true //true: step #2 of 6
            , goodChallenge) == -1)
         {
            //auth setup failure, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_REPLY data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AUTH_REPLY, authreplyBuf.getRef()
            , true))
         {
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_REPLY not sent");
            return false;
         }

         break;
      }

      case ZMQ_MSGTYPE_AUTH_PROPOSE:
      {
         bool goodPropose = true;
         auto proposeResult =
            connection->encData_->processAuthpropose(
            dataBdr.getPtr(), dataBdr.getSize());

         if (proposeResult == -1)
         {
            //auth setup failure, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_PROPOSE processing failed");
            return false;
         }
         else if (proposeResult == 1)
         {
            goodPropose = false;
         }
         else
         {
            //keep track of the propose check state
            connection->encData_->setGoodPropose();
         }

         BinaryData authchallengeBuf(BIP151PRVKEYSIZE);
         if (connection->encData_->getAuthchallengeData(
            authchallengeBuf.getPtr(), authchallengeBuf.getSize()
            , "" //empty string, use chosen key from processing auth propose
            , false //false: step #4 of 6
            , goodPropose) == -1)
         {
            //auth setup failure, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_CHALLENGE data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AUTH_CHALLENGE
            , authchallengeBuf.getRef(), true))
         {
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_CHALLENGE not sent");
         }

         break;
      }

      case ZMQ_MSGTYPE_AUTH_REPLY:
      {
         if (connection->encData_->processAuthreply(dataBdr.getPtr()
            , dataBdr.getSize(), false
            , connection->encData_->getProposeFlag()) != 0)
         {
            //invalid auth setup, kill connection
            return false;
         }

         //rekey after succesful BIP150 handshake
         connection->encData_->bip150HandshakeRekey();
         connection->bip150HandshakeCompleted_ = true;
         notifyListenerOnNewConnection(clientID);

         logger_->info("[processHandshake] BIP 150 handshake with client "
            "complete - connection with {} is ready and fully secured"
            , BinaryData(clientID).toHexStr());

         break;
      }

      case ZMQ_MSGTYPE_DISCONNECT:
         logger_->debug("[processHandshake] disconnect request received from {}"
            , BinaryData(clientID).toHexStr());
         resetBIP151Connection(clientID);
         notifyListenerOnDisconnectedClient(clientID);
         break;

      default:
         logger_->error("[processHandshake] Unknown message type.");
         return false;
      }

      return true;
   };

   if (clientID.empty()) {
      logger_->error("[{}] empty client ID", __func__);
      return false;
   }
   bool retVal = processHandshake();
   if (!retVal) {
      logger_->error("[{}] BIP 150/151 handshake process failed.", __func__);
   }
   return retVal;
}

// Function used to reset the BIP 150/151 handshake data. Called when a
// connection is shut down.
//
// INPUT:  The client ID. (const string&)
// OUTPUT: None
// RETURN: None
void ZmqBIP15XServerConnection::resetBIP151Connection(const string& clientID)
{
   BinaryData hexID{clientID};

   bool connectionErased = false;

   {
      FastLock locker{connectionsLock_};
      auto it = socketConnMap_.find(clientID);
      if (it != socketConnMap_.end()) {
         socketConnMap_.erase(it);
         connectionErased = true;
      } else {
         connectionErased = false;
      }
   }

   if (connectionErased) {
      FastLock locker{heartbeatsLock_};
      auto it = lastHeartbeats_.find(clientID);
      if (it != lastHeartbeats_.end()) {
         lastHeartbeats_.erase(it);
      } else {
         logger_->error("[ZmqBIP15XServerConnection::resetBIP151Connection] there are no heartbeat timer for connection {} to be erased"
            , hexID.toHexStr());
      }
   }

   if (connectionErased) {
      logger_->debug("[ZmqBIP15XServerConnection::resetBIP151Connection] Connection ID {} erased"
            , hexID.toHexStr());
   } else {
      logger_->error("[ZmqBIP15XServerConnection::resetBIP151Connection] Connection ID {} not found"
            , hexID.toHexStr());
   }
}

// Function used to set the BIP 150/151 handshake data. Called when a connection
// is created.
//
// INPUT:  The client ID. (const string&)
// OUTPUT: None
// RETURN: new or existing connection
std::shared_ptr<ZmqBIP15XPerConnData> ZmqBIP15XServerConnection::setBIP151Connection(
   const string& clientID)
{
   auto connection = GetConnection(clientID);
   if (nullptr != connection) {
      return connection;
   }

   assert(cbTrustedClients_);
   auto lbds = getAuthPeerLambda();
   for (const auto &b : cbTrustedClients_()) {
      const auto colonIndex = b.find(':');
      if (colonIndex == std::string::npos) {
         logger_->error("[{}] Trusted client list is malformed (for {})."
            , __func__, b);
         return nullptr;
      }

      std::string keyHex = b.substr(colonIndex + 1);

      try {
         const BinaryData inKey = READHEX(keyHex);
         authPeers_->addPeer(inKey, vector<string>{ clientID });
      }
      catch (const std::exception &e) {
         const BinaryData cID(clientID);
         logger_->error("[{}] Trusted identity key for client {} is not "
            "accepted: {}", __func__, cID.toHexStr(), e.what());
            return nullptr;
      }
   }

   connection = std::make_shared<ZmqBIP15XPerConnData>();
   connection->encData_ = make_unique<BIP151Connection>(lbds);
   connection->outKeyTimePoint_ = chrono::steady_clock::now();

   // XXX add connection
   AddConnection(clientID, connection);

   UpdateClientHeartbeatTimestamp(clientID);

   return connection;
}

bool ZmqBIP15XServerConnection::AddConnection(const std::string& clientId, const std::shared_ptr<ZmqBIP15XPerConnData>& connection)
{
   {
      FastLock locker{connectionsLock_};

      auto it = socketConnMap_.find(clientId);
      if (it != socketConnMap_.end()) {
         logger_->error("[ZmqBIP15XServerConnection::AddConnection] connection already saved for {}", BinaryData(clientId).toHexStr());
         return false;
      }

      socketConnMap_.emplace(clientId, connection);
   }

   logger_->debug("[ZmqBIP15XServerConnection::AddConnection] adding new connection for client {}", BinaryData(clientId).toHexStr());
   return true;
}

std::shared_ptr<ZmqBIP15XPerConnData> ZmqBIP15XServerConnection::GetConnection(const std::string& clientId)
{
   FastLock locker{connectionsLock_};

   auto it = socketConnMap_.find(clientId);
   if (it == socketConnMap_.end()) {
      return nullptr;
   }

   return it->second;
}

void ZmqBIP15XServerConnection::UpdateClientHeartbeatTimestamp(const std::string& clientId)
{
   FastLock locker{heartbeatsLock_};

   auto currentTime = std::chrono::steady_clock::now();

   auto it = lastHeartbeats_.find(clientId);
   if (it == lastHeartbeats_.end()) {
      lastHeartbeats_.emplace(clientId, currentTime);
      logger_->debug("[ZmqBIP15XServerConnection::UpdateClientHeartbeatTimestamp] added {} HT: {}"
         , BinaryData(clientId).toHexStr()
         , std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count());
   } else {
      it->second = currentTime;
   }
}

// Get lambda functions related to authorized peers. Copied from Armory.
//
// INPUT:  None
// OUTPUT: None
// RETURN: AuthPeersLambdas object with required lambdas.
AuthPeersLambdas ZmqBIP15XServerConnection::getAuthPeerLambda()
{
   auto authPeerPtr = authPeers_;

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

// Generate a cookie with the signer's identity public key.
//
// INPUT:  N/A
// OUTPUT: N/A
// RETURN: True if success, false if failure.
bool ZmqBIP15XServerConnection::genBIPIDCookie()
{
   if (SystemFileUtils::fileExist(bipIDCookiePath_)) {
      if (!SystemFileUtils::rmFile(bipIDCookiePath_)) {
         logger_->error("[{}] Unable to delete server identity cookie ({}). "
            "Will not write a new cookie.", __func__, bipIDCookiePath_);
         return false;
      }
   }

   // Ensure that we only write the compressed key.
   ofstream cookieFile(bipIDCookiePath_, ios::out | ios::binary);
   const BinaryData ourIDKey = getOwnPubKey();
   if (ourIDKey.getSize() != BTC_ECKEY_COMPRESSED_LENGTH) {
      logger_->error("[{}] Server identity key ({}) is uncompressed. Will not "
         "write the identity cookie.", __func__, bipIDCookiePath_);
      return false;
   }

   logger_->debug("[{}] Writing a new server identity cookie ({}).", __func__
      ,  bipIDCookiePath_);
   cookieFile.write(getOwnPubKey().getCharPtr(), BTC_ECKEY_COMPRESSED_LENGTH);
   cookieFile.close();

   return true;
}

void ZmqBIP15XServerConnection::addAuthPeer(const BinaryData& inKey
   , const std::string& keyName)
{
   if (!(CryptoECDSA().VerifyPublicKeyValid(inKey))) {
      logger_->error("[{}] BIP 150 authorized key ({}) for user {} is invalid."
         , __func__,  inKey.toHexStr(), keyName);
      return;
   }
   authPeers_->addPeer(inKey, vector<string>{ keyName });
}

void ZmqBIP15XServerConnection::updatePeerKeys(const std::vector<std::pair<std::string, BinaryData>> &keys)
{
   const auto peers = authPeers_->getPeerNameMap();
   for (const auto &peer : peers) {
      try {
         authPeers_->eraseName(peer.first);
      }
      catch (const AuthorizedPeersException &) {} // just ignore exception when erasing "own" key
      catch (const std::exception &e) {
         logger_->error("[{}] exception when erasing peer key for {}: {}", __func__
            , peer.first, e.what());
      }
      catch (...) {
         logger_->error("[{}] exception when erasing peer key for {}", __func__, peer.first);
      }
   }
   for (const auto &key : keys) {
      if (!(CryptoECDSA().VerifyPublicKeyValid(key.second))) {
         logger_->error("[{}] BIP 150 authorized key ({}) for user {} is invalid."
            , __func__, key.second.toHexStr(), key.first);
         continue;
      }
      try {
         authPeers_->addPeer(key.second, vector<string>{ key.first });
      }
      catch (const std::exception &e) {
         logger_->error("[{}] failed to add peer {}: {}", __func__, key.first, e.what());
      }
   }
}

// Get the client's identity public key. Intended for use with local clients.
//
// INPUT:  The accompanying key IP:Port or name. (const string)
// OUTPUT: The buffer that will hold the compressed ID key. (BinaryData)
// RETURN: True if success, false if failure.
bool ZmqBIP15XServerConnection::getClientIDCookie(BinaryData& cookieBuf)
{
   if (!useClientIDCookie_) {
      logger_->error("[{}] Client identity cookie requested despite not being "
         "available.", __func__);
      return false;
   }

   if (!SystemFileUtils::fileExist(bipIDCookiePath_)) {
      logger_->error("[{}] Client identity cookie ({}) doesn't exist. Unable "
         "to verify server identity.", __func__, bipIDCookiePath_);
      return false;
   }

   // Ensure that we only read a compressed key.
   ifstream cookieFile(bipIDCookiePath_, ios::in | ios::binary);
   cookieFile.read(cookieBuf.getCharPtr(), BIP151PUBKEYSIZE);
   cookieFile.close();
   if (!(CryptoECDSA().VerifyPublicKeyValid(cookieBuf))) {
      logger_->error("[{}] Client identity key ({}) isn't a valid compressed "
         "key. Unable to verify server identity.", __func__
         , cookieBuf.toHexStr());
      return false;
   }

   return true;
}

// Get the server's compressed BIP 150 identity public key.
//
// INPUT:  N/A
// OUTPUT: N/A
// RETURN: A buffer with the compressed ECDSA ID pub key. (BinaryData)
BinaryData ZmqBIP15XServerConnection::getOwnPubKey() const
{
   const auto pubKey = authPeers_->getOwnPublicKey();
   return SecureBinaryData(pubKey.pubkey, pubKey.compressed
      ? BTC_ECKEY_COMPRESSED_LENGTH : BTC_ECKEY_UNCOMPRESSED_LENGTH);
}
