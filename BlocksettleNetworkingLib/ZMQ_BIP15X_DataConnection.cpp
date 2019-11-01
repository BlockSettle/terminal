#include "ZMQ_BIP15X_DataConnection.h"

#include <chrono>

#include "BIP150_151.h"
#include "EncryptionUtils.h"
#include "FastLock.h"
#include "FutureValue.h"
#include "MessageHolder.h"
#include "SystemFileUtils.h"
#include "ThreadName.h"
#include "ZMQ_BIP15X_Msg.h"
#include "ZMQ_BIP15X_ServerConnection.h"
#include "ZmqHelperFunctions.h"

using namespace std;

namespace
{
   const int HEARTBEAT_PACKET_SIZE = 23;

   const int ControlSocketIndex = 0;
   const int StreamSocketIndex = 1;
   const int MonitorSocketIndex = 2;

} // namespace


ZmqBIP15XDataConnectionParams::ZmqBIP15XDataConnectionParams()
{
   heartbeatInterval = ZmqBIP15XServerConnection::getDefaultHeartbeatInterval();
}

void ZmqBIP15XDataConnectionParams::setLocalHeartbeatInterval()
{
   heartbeatInterval = ZmqBIP15XServerConnection::getLocalHeartbeatInterval();
}

// The constructor to use.
//
// INPUT:  Logger object. (const shared_ptr<spdlog::logger>&)
//         Params. (ZmqBIP15XDataConnectionParams)
// OUTPUT: None
ZmqBIP15XDataConnection::ZmqBIP15XDataConnection(const shared_ptr<spdlog::logger>& logger
   , const ZmqBIP15XDataConnectionParams &params)
   : logger_(logger)
   , params_(params)
   // There is some obscure problem with ZMQ if same context reused:
   // ZMQ recreates TCP connections for closed ZMQ sockets.
   // Using new context fixes this problem.
   , context_(new ZmqContext(logger))
   , dataSocket_(ZmqContext::CreateNullSocket())
   , monSocket_(ZmqContext::CreateNullSocket())
   , threadMasterSocket_(ZmqContext::CreateNullSocket())
   , threadSlaveSocket_(ZmqContext::CreateNullSocket())
{
   assert(logger_);

   if (!params.ephemeralPeers && (params.ownKeyFileDir.empty() || params.ownKeyFileName.empty())) {
      throw std::runtime_error("Client requested static ID key but no key " \
         "wallet file is specified.");
   }

   if (params_.cookie != BIP15XCookie::NotUsed && params_.cookiePath.empty()) {
      throw std::runtime_error("ID cookie creation requested but no name " \
         "supplied. Connection is incomplete.");
   }

   outKeyTimePoint_ = chrono::steady_clock::now();

   // In general, load the server key from a special Armory wallet file.
   if (!params.ephemeralPeers) {
      authPeers_ = std::make_unique<AuthorizedPeers>(
         params.ownKeyFileDir, params.ownKeyFileName);
   }
   else {
      authPeers_ = std::make_unique<AuthorizedPeers>();
   }

   if (params_.cookie == BIP15XCookie::MakeClient) {
      genBIPIDCookie();
   }
}

ZmqBIP15XDataConnection::~ZmqBIP15XDataConnection() noexcept
{
   // If it exists, delete the identity cookie.
   if (params_.cookie == BIP15XCookie::MakeClient) {
//      const string absCookiePath =
//         SystemFilePaths::appDataLocation() + "/" + bipIDCookieName_;
      if (SystemFileUtils::fileExist(params_.cookiePath)) {
         if (!SystemFileUtils::rmFile(params_.cookiePath)) {
            logger_->error("[{}] Unable to delete client identity cookie ({})."
               , __func__, params_.cookiePath);
         }
      }
   }

   // Need to close connection before ZmqBIP15XDataConnection is partially destroyed!
   // Otherwise it might crash in ZmqBIP15XDataConnection::ProcessIncomingData a bit later
   closeConnection();
}

// Get lambda functions related to authorized peers. Copied from Armory.
//
// INPUT:  None
// OUTPUT: None
// RETURN: AuthPeersLambdas object with required lambdas.
AuthPeersLambdas ZmqBIP15XDataConnection::getAuthPeerLambda() const
{
   auto getMap = [this](void) -> const map<string, btc_pubkey>& {
      std::lock_guard<std::mutex> lock(authPeersMutex_);
      return authPeers_->getPeerNameMap();
   };

   auto getPrivKey = [this](const BinaryDataRef& pubkey) -> const SecureBinaryData& {
      std::lock_guard<std::mutex> lock(authPeersMutex_);
      return authPeers_->getPrivateKey(pubkey);
   };

   auto getAuthSet = [this](void) -> const set<SecureBinaryData>& {
      std::lock_guard<std::mutex> lock(authPeersMutex_);
      return authPeers_->getPublicKeySet();
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
void ZmqBIP15XDataConnection::rekeyIfNeeded(size_t dataSize)
{
   bool needsRekey = false;
   const auto rightNow = chrono::steady_clock::now();

   if (bip150HandshakeCompleted_) {
      // Rekey off # of bytes sent or length of time since last rekey.
      if (bip151Connection_->rekeyNeeded(dataSize)) {
         needsRekey = true;
      }
      else {
         auto time_sec = chrono::duration_cast<chrono::seconds>(
            rightNow - outKeyTimePoint_);
         if (time_sec.count() >= ZMQ_AEAD_REKEY_INVERVAL_SECS) {
            needsRekey = true;
         }
      }

      if (needsRekey) {
         outKeyTimePoint_ = rightNow;
         rekey();
      }
   }
}

void ZmqBIP15XDataConnection::listenFunction()
{
   bs::setCurrentThreadName(params_.threadName);

   zmq_pollitem_t  poll_items[3];
   memset(&poll_items, 0, sizeof(poll_items));

   poll_items[ControlSocketIndex].socket = threadSlaveSocket_.get();
   poll_items[ControlSocketIndex].events = ZMQ_POLLIN;

   poll_items[StreamSocketIndex].socket = dataSocket_.get();
   poll_items[StreamSocketIndex].events = ZMQ_POLLIN;

   poll_items[MonitorSocketIndex].socket = monSocket_.get();
   poll_items[MonitorSocketIndex].events = ZMQ_POLLIN;

   SPDLOG_LOGGER_DEBUG(logger_, "[{}] poll thread started for {}", __func__
      , connectionName_);

   bool tcpConnected = false;
   bool stopThread = false;

   auto connectionStarted = std::chrono::steady_clock::now();

   while (!fatalError_ && !stopThread) {
      // Wake up from time to time to check heartbeats and connection timeout.
      // periodMs should be small enough.
      auto periodMs = isConnected_ ? (params_.heartbeatInterval / std::chrono::milliseconds(1) / 5)
                                   : (params_.connectionTimeout / std::chrono::milliseconds(1) / 10);

      int result = zmq_poll(poll_items, 3, std::max(1, int(periodMs)));
      if (result == -1) {
         logger_->error("[{}] poll failed for {} : {}", __func__
            , connectionName_, zmq_strerror(zmq_errno()));
         break;
      }

      if (!isConnected_ && std::chrono::steady_clock::now() - connectionStarted > params_.connectionTimeout) {
         if (bip151HandshakeCompleted_ && !bip150HandshakeCompleted_) {
            SPDLOG_LOGGER_ERROR(logger_, "ZMQ BIP connection is timed out (bip151 was completed, probaly client credential is not valid)");
            onError(DataConnectionListener::HandshakeFailed);
         } else {
            SPDLOG_LOGGER_ERROR(logger_, "ZMQ BIP connection is timed out");
            onError(DataConnectionListener::ConnectionTimeout);
         }
      }

      triggerHeartbeatCheck();

      if (poll_items[ControlSocketIndex].revents & ZMQ_POLLIN) {
         MessageHolder   command;

         int recvResult = zmq_msg_recv(&command, poll_items[ControlSocketIndex].socket, ZMQ_DONTWAIT);
         if (recvResult == -1) {
            logger_->error("[{}] failed to recv command on {} : {}", __func__
               , connectionName_, zmq_strerror(zmq_errno()));
            break;
         }

         switch (InternalCommandCode(command.ToInt())) {
            case InternalCommandCode::Send:
               sendPendingData();
               break;
            case InternalCommandCode::Stop:
               sendDisconnectMsg();
               stopThread = true;
               break;
            default:
               assert(false);
         }
      }

      if (poll_items[StreamSocketIndex].revents & ZMQ_POLLIN) {
         if (!recvData()) {
            break;
         }
      }

      if (poll_items[MonitorSocketIndex].revents & ZMQ_POLLIN) {
         switch (bs::network::get_monitor_event(monSocket_.get())) {
         case ZMQ_EVENT_CONNECTED:
            if (!tcpConnected) {
               startBIP151Handshake();
               tcpConnected = true;
            }
            break;

         case ZMQ_EVENT_DISCONNECTED:
            if (isConnected_) {
               onDisconnected();
            }
            break;
         default:
            break;
         }
      }

      // Try to send pending data after callbacks if any
      sendPendingData();
   }

   zmq_socket_monitor(dataSocket_.get(), nullptr, ZMQ_EVENT_ALL);

   if (isConnected_) {
      onDisconnected();
   }
}

void ZmqBIP15XDataConnection::resetConnectionObjects()
{
   // do not clean connectionName_ for debug purpose
   socketId_.clear();

   dataSocket_.reset();
   threadMasterSocket_.reset();
   threadSlaveSocket_.reset();
}

bool ZmqBIP15XDataConnection::ConfigureDataSocket(const ZmqContext::sock_ptr &socket)
{
   int lingerPeriod = 0;
   int result = zmq_setsockopt(socket.get(), ZMQ_LINGER, &lingerPeriod, sizeof(lingerPeriod));
   if (result != 0) {
      logger_->error("[{}] {} failed to set linger interval: {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }
   return true;
}

void ZmqBIP15XDataConnection::rekey()
{
   logger_->debug("[ZmqBIP15XDataConnection::{}] rekeying", __func__);

   if (!bip150HandshakeCompleted_) {
      logger_->error("[ZmqBIP15XDataConnection::{}] Can't rekey before BIP150 "
         "handshake is complete", __func__);
      return;
   }

   BinaryData rekeyData(BIP151PUBKEYSIZE);
   memset(rekeyData.getPtr(), 0, BIP151PUBKEYSIZE);

   auto packet = ZmqBipMsgBuilder(rekeyData.getRef()
      , ZMQ_MSGTYPE_AEAD_REKEY).encryptIfNeeded(bip151Connection_.get()).build();
   sendPacket(packet);
   bip151Connection_->rekeyOuterSession();
   ++outerRekeyCount_;
}

bool ZmqBIP15XDataConnection::isActive() const
{
   return dataSocket_ != nullptr;
}

// An internal send function to be used when this class constructs a packet. The
// packet must already be encrypted (if required) and ready to be sent.
//
// ***Please use this function for sending all data from inside this class.***
//
// INPUT:  The data to send. (const string&)
// OUTPUT: None
// RETURN: True if success, false if failure.
void ZmqBIP15XDataConnection::sendPacket(const BinaryData& data)
{
   if (fatalError_) {
      return;
   }

   int result = zmq_send(dataSocket_.get(), data.getPtr(), data.getSize(), 0);
   assert(result == int(data.getSize()));
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
   if (fatalError_) {
      return false;
   }

   {
      std::lock_guard<std::mutex> lock(pendingDataMutex_);
      pendingData_.push_back(data);
   }

   // Notify listening thread that there is new data.
   // If this is called from listening thread pendingData_ will be processed right after callbacks.
   if (std::this_thread::get_id() != listenThread_.get_id()) {
      sendCommand(InternalCommandCode::Send);
   }

   return true;
}

// A function that is used to trigger heartbeats. Required because ZMQ is unable
// to tell, via a data socket connection, when a client has disconnected.
//
// INPUT:  N/A
// OUTPUT: N/A
// RETURN: N/A
void ZmqBIP15XDataConnection::triggerHeartbeatCheck()
{
   if (!bip151HandshakeCompleted_) {
      return;
   }

   const auto now = std::chrono::steady_clock::now();
   const auto idlePeriod = now - lastHeartbeatSend_;
   if (idlePeriod < params_.heartbeatInterval) {
      return;
   }
   lastHeartbeatSend_ = now;

   // If a rekey is needed, rekey before encrypting. Estimate the size of the
   // final packet first in order to get the # of bytes transmitted.
   rekeyIfNeeded(HEARTBEAT_PACKET_SIZE);

   auto packet = ZmqBipMsgBuilder(ZMQ_MSGTYPE_HEARTBEAT)
      .encryptIfNeeded(bip151Connection_.get()).build();

   // An error message is already logged elsewhere if the send fails.
   // sendPacket already sets the timestamp.
   sendPacket(packet);

   if (idlePeriod > params_.heartbeatInterval * 2) {
      logger_->debug("[ZmqBIP15XDataConnection:{}] hibernation detected, reset server's last timestamp", __func__);
      lastHeartbeatReply_ = now;
      return;
   }

   auto lastHeartbeatDiff = now - lastHeartbeatReply_;
   if (lastHeartbeatDiff > params_.heartbeatInterval * 2) {
      onError(DataConnectionListener::HeartbeatWaitFailed);
   }
}

void ZmqBIP15XDataConnection::onConnected()
{
   assert(std::this_thread::get_id() == listenThread_.get_id());
   assert(!isConnected_);
   isConnected_ = true;
   notifyOnConnected();
}

void ZmqBIP15XDataConnection::onDisconnected()
{
   assert(std::this_thread::get_id() == listenThread_.get_id());
   assert(isConnected_);
   isConnected_ = false;
   notifyOnDisconnected();
}

void ZmqBIP15XDataConnection::onError(DataConnectionListener::DataConnectionError errorCode)
{
   assert(std::this_thread::get_id() == listenThread_.get_id());

   // Notify about error only once
   if (fatalError_) {
      return;
   }

   if (isConnected_) {
      onDisconnected();
   }

   fatalError_ = true;
   notifyOnError(errorCode);
}

bool ZmqBIP15XDataConnection::SetZMQTransport(ZMQTransport transport)
{
   switch(transport) {
   case ZMQTransport::TCPTransport:
   case ZMQTransport::InprocTransport:
      zmqTransport_ = transport;
      return true;
   }

   logger_->error("[{}] undefined transport", __func__);
   return false;
}

// static
BinaryData ZmqBIP15XDataConnection::getOwnPubKey(const string &ownKeyFileDir, const string &ownKeyFileName)
{
   return ZmqBIP15XServerConnection::getOwnPubKey(ownKeyFileDir, ownKeyFileName);
}

// static
BinaryData ZmqBIP15XDataConnection::getOwnPubKey(const AuthorizedPeers &authPeers)
{
   return ZmqBIP15XServerConnection::getOwnPubKey(authPeers);
}

// Kick off the BIP 151 handshake. This is the first function to call once the
// unencrypted connection is established.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::startBIP151Handshake()
{
   auto packet = ZmqBipMsgBuilder(ZMQ_MSGTYPE_AEAD_SETUP).build();
   sendPacket(packet);
   return true;
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

   if (!bip151Connection_) {
      logger_->error("[{}] received {} bytes of data in disconnected state"
         , __func__, rawData.size());
      return;
   }

   // Perform decryption if we're ready.
   if (bip151Connection_->connectionComplete()) {
      auto result = bip151Connection_->decryptPacket(
         payload.getPtr(), payload.getSize(),
         payload.getPtr(), payload.getSize());

      if (result != 0) {
         logger_->error("[ZmqBIP15XDataConnection::{}] Packet [{} bytes] "
            "from {} decryption failed - Error {}"
            , __func__, payload.getSize(), connectionName_, result);
         onError(DataConnectionListener::SerializationFailed);
         return;
      }

      payload.resize(payload.getSize() - POLY1305MACLEN);
   }

   ProcessIncomingData(payload);
}

bool ZmqBIP15XDataConnection::openConnection(const std::string &host
   , const std::string &port, DataConnectionListener *listener)
{
   // BIP 151 connection setup. Technically should be per-socket or something
   // similar but data connections will only connect to one machine at a time.
   auto lbds = getAuthPeerLambda();
   bip151Connection_ = std::make_unique<BIP151Connection>(lbds);
   assert(context_ != nullptr);
   assert(listener != nullptr);

   if (isActive()) {
      logger_->error("[{}] connection active. You should close it first: {}."
         , __func__, connectionName_);
      return false;
   }

   isConnected_ = false;
   fatalError_ = false;
   serverSendsHeartbeat_ = false;

   hostAddr_ = host;
   hostPort_ = port;
   std::string tempConnectionName = context_->GenerateConnectionName(host, port);

   char buf[256];
   size_t  buf_size = 256;

   // create stream socket ( connected to server )
   ZmqContext::sock_ptr tempDataSocket = CreateDataSocket();
   assert(tempDataSocket);

   if (!ConfigureDataSocket(tempDataSocket)) {
      logger_->error("[{}] failed to configure data socket socket {}"
         , __func__, tempConnectionName);
      return false;
   }

   // connect socket to server ( connection state will be changed in listen thread )
   std::string endpoint = ZmqContext::CreateConnectionEndpoint(zmqTransport_, host, port);
   if (endpoint.empty()) {
      logger_->error("[{}] failed to generate connection address", __func__);
      return false;
   }

   int result = 0;
   std::string controlEndpoint = std::string("inproc://") + tempConnectionName;

   // create master and slave paired sockets to control connection and resend data
   ZmqContext::sock_ptr tempThreadMasterSocket = context_->CreateInternalControlSocket();
   assert(tempThreadMasterSocket);

   result = zmq_bind(tempThreadMasterSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[{}] failed to bind ThreadMasterSocket socket {}: {}"
         , __func__, tempConnectionName, zmq_strerror(zmq_errno()));
      return false;
   }

   ZmqContext::sock_ptr tempThreadSlaveSocket = context_->CreateInternalControlSocket();
   assert(tempThreadSlaveSocket);

   result = zmq_connect(tempThreadSlaveSocket.get(), controlEndpoint.c_str());
   if (result != 0) {
      logger_->error("[{}] failed to connect ThreadSlaveSocket socket {}"
         , __func__, tempConnectionName);
      return false;
   }

   int rc = zmq_socket_monitor(tempDataSocket.get(), ("inproc://mon-" + tempConnectionName).c_str(), ZMQ_EVENT_ALL);
   if (rc != 0) {
      logger_->error("[{}] Failed to create monitor socket: {}", __func__
         , zmq_strerror(zmq_errno()));
      return false;
   }
   auto tempMonSocket = context_->CreateMonitorSocket();
   rc = zmq_connect(tempMonSocket.get(), ("inproc://mon-" + tempConnectionName).c_str());
   if (rc != 0) {
      logger_->error("[{}] Failed to connect monitor socket: {}", __func__
         , zmq_strerror(zmq_errno()));
      return false;
   }

   monSocket_ = std::move(tempMonSocket);

   result = zmq_connect(tempDataSocket.get(), endpoint.c_str());
   if (result != 0) {
      logger_->error("[{}] failed to connect socket to {}", __func__
         , endpoint);
      return false;
   }

   // get socket id
   result = zmq_getsockopt(tempDataSocket.get(), ZMQ_IDENTITY, buf, &buf_size);
   if (result != 0) {
      logger_->error("[{}] failed to get socket Id {}", __func__
         , tempConnectionName);
      return false;
   }

   // ok, move temp data to members
   connectionName_ = std::move(tempConnectionName);
   socketId_ = std::string(buf, buf_size);
   dataSocket_ = std::move(tempDataSocket);
   threadMasterSocket_ = std::move(tempThreadMasterSocket);
   threadSlaveSocket_ = std::move(tempThreadSlaveSocket);

   setListener(listener);

   // and start thread
   listenThread_ = std::thread(&ZmqBIP15XDataConnection::listenFunction, this);

   SPDLOG_LOGGER_DEBUG(logger_, "[{}] starting connection for {}", __func__
      , connectionName_);
   return true;
}

// Close the connection.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::closeConnection()
{
   // Do not call from callbacks!
   assert(std::this_thread::get_id() != listenThread_.get_id());

   if (!isActive()) {
      SPDLOG_LOGGER_DEBUG(logger_, "[{}] connection already stopped {}", __func__
         , connectionName_);
      return true;
   }

   // If a future obj is still waiting, satisfy it to prevent lockup. This
   // shouldn't happen here but it's an emergency fallback.
   if (serverPubkeyProm_) {
      serverPubkeyProm_->setValue(false);
   }

   SPDLOG_LOGGER_DEBUG(logger_, "[{}] stopping {}", __func__, connectionName_);

   sendCommand(InternalCommandCode::Stop);
   listenThread_.join();

   resetConnectionObjects();

   bip151Connection_.reset();
   pendingData_.clear();
   bip150HandshakeCompleted_ = false;
   bip151HandshakeCompleted_ = false;
   return true;
}

// The function that processes raw ZMQ connection data. It processes the BIP
// 150/151 handshake (if necessary) and decrypts the raw data.
//
// INPUT:  Reformed message. (const BinaryData&) // TODO: FIX UP MSG
// OUTPUT: None
// RETURN: None
void ZmqBIP15XDataConnection::ProcessIncomingData(BinaryData& payload)
{
   ZmqBipMsg packet = ZmqBipMsg::parsePacket(payload);
   if (!packet.isValid()) {
      logger_->error("[ZmqBIP15XDataConnection::{}] Deserialization failed "
         "(connection {})", __func__, connectionName_);
      onError(DataConnectionListener::SerializationFailed);
      return;
   }

   if (packet.getType() == ZMQ_MSGTYPE_HEARTBEAT) {
      lastHeartbeatReply_ = std::chrono::steady_clock::now();
      serverSendsHeartbeat_ = true;
      return;
   }

   // If we're still handshaking, take the next step. (No fragments allowed.)
   if (packet.getType() > ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      if (!processAEADHandshake(packet)) {
         logger_->error("[ZmqBIP15XDataConnection::{}] Handshake failed "
            "(connection {})", __func__, connectionName_);

         onError(DataConnectionListener::HandshakeFailed);
         return;
      }

      return;
   }

   // We can now safely obtain the full message.
   BinaryDataRef inMsg = packet.getData();

   // We shouldn't get here but just in case....
   if (!bip151Connection_ || (bip151Connection_->getBIP150State() != BIP150State::SUCCESS)) {
      logger_->error("[ZmqBIP15XDataConnection::{}] Encryption handshake "
         "is incomplete (connection {})", __func__, connectionName_);
      if (bip151Connection_) {
         onError(DataConnectionListener::HandshakeFailed);
      }
      return;
   }

   // Pass the final data up the chain.
   notifyOnData(inMsg.toBinStr());
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
      logger_->error("[ZmqBIP15XDataConnection::{}] {} failed to recv data "
         "frame from stream: {}" , __func__, connectionName_
         , zmq_strerror(zmq_errno()));
      return false;
   }

   // Process the raw data.
   onRawDataReceived(data.ToString());
   return true;
}

// The function processing the BIP 150/151 handshake packets.
//
// INPUT:  The handshake packet. (const ZmqBIP15XMsg&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::processAEADHandshake(
   const ZmqBipMsg& msgObj)
{
   // Function used to send data out on the wire.
   auto writeData = [this](BinaryData& payload, uint8_t type, bool encrypt) {
      auto conn = encrypt ? bip151Connection_.get() : nullptr;
      auto packet = ZmqBipMsgBuilder(payload, type).encryptIfNeeded(conn).build();
      sendPacket(packet);
   };

   //compute server name
   stringstream ss;
   ss << hostAddr_ << ":" << hostPort_;
   const auto srvId = ss.str();

   // Read the message, get the type, and process as needed. Code mostly copied
   // from Armory.
   auto msgbdr = msgObj.getData();
   switch (msgObj.getType()) {
   case ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY:
   {
      /*packet is server's pubkey, do we have it?*/

      //init server promise
      serverPubkeyProm_ = std::make_shared<FutureValue<bool>>();

      // If it's a local connection, get a cookie with the server's key.
      if (params_.cookie == BIP15XCookie::ReadServer) {
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

            std::lock_guard<std::mutex> lock(authPeersMutex_);
            authPeers_->eraseName(localAddrV4);
            authPeers_->addPeer(cookieKey, keyName);
         }
      }

      // If we don't have the key already, we may ask the the user if they wish
      // to continue. (Remote signer only.)
      if (!bip151Connection_->havePublicKey(msgbdr, srvId)) {
         //we don't have this key, call user prompt lambda
         if (verifyNewIDKey(msgbdr, srvId)) {
            // Add the key. Old keys aren't deleted automatically. Do it to be safe.
            std::lock_guard<std::mutex> lock(authPeersMutex_);
            authPeers_->eraseName(srvId);
            authPeers_->addPeer(msgbdr.copy(), std::vector<std::string>{ srvId });
         }
      }
      else {
         //set server key promise
         if (serverPubkeyProm_) {
            serverPubkeyProm_->setValue(true);
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
         bool result = serverPubkeyProm_->waitValue();

         if (result) {
            serverPubkeyProm_.reset();
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

      if (challengeResult == 1) {
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

      auto now = chrono::steady_clock::now();
      outKeyTimePoint_ = now;
      lastHeartbeatReply_ = now;
      lastHeartbeatSend_ = now;

      logger_->debug("[processHandshake] BIP 150 handshake with server complete "
         "- connection to {} is ready and fully secured", srvId);

      onConnected();
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
void ZmqBIP15XDataConnection::setCBs(const ZmqBipNewKeyCb& inNewKeyCB) {
   if (!inNewKeyCB) {
      cbNewKey_ = [this](const std::string &, const std::string, const std::string&
            , const std::shared_ptr<FutureValue<bool>> &prom) {
         SPDLOG_LOGGER_DEBUG(logger_, "no new key callback was set - auto-accepting connections");
         prom->setValue(true);
      };
      return;
   }

   cbNewKey_ = inNewKeyCB;
}

// Add an authorized peer's BIP 150 identity key manually.
//
// INPUT:  The authorized key. (const BinaryData)
//         The server IP address (or host name) and port. (const string)
// OUTPUT: N/A
// RETURN: N/A
void ZmqBIP15XDataConnection::addAuthPeer(const ZmqBIP15XPeer &peer)
{
   std::lock_guard<std::mutex> lock(authPeersMutex_);
   ZmqBIP15XUtils::addAuthPeer(authPeers_.get(), peer);
}

void ZmqBIP15XDataConnection::updatePeerKeys(const ZmqBIP15XPeers &peers)
{
   std::lock_guard<std::mutex> lock(authPeersMutex_);
   ZmqBIP15XUtils::updatePeerKeys(authPeers_.get(), peers);
}

// If the user is presented with a new remote server ID key it doesn't already
// know about, verify the key. A promise will also be used in case any functions
// are waiting on verification results.
//
// INPUT:  The key to verify. (const BinaryDataRef)
//         The server IP address (or host name) and port. (const string)
// OUTPUT: N/A
// RETURN: N/A
bool ZmqBIP15XDataConnection::verifyNewIDKey(const BinaryDataRef& newKey
   , const string& srvAddrPort)
{
   if (params_.cookie == BIP15XCookie::ReadServer) {
      // If we get here, it's because the cookie add failed or the cookie was
      // incorrect. Satisfy the promise to prevent lockup.
      logger_->error("[{}] Server ID key cookie could not be verified", __func__);
      serverPubkeyProm_->setValue(false);
      onError(DataConnectionListener::HandshakeFailed);
      return false;
   }

   logger_->debug("[{}] New key ({}) for server [{}] arrived.", __func__
      , newKey.toHexStr(), srvAddrPort);

   if (!cbNewKey_) {
      logger_->error("[{}] no server key callback is set - aborting handshake", __func__);
      onError(DataConnectionListener::HandshakeFailed);
      return false;
   }

   // Ask the user if they wish to accept the new identity key.
   // There shouldn't be any old key, at least in authPeerNameSearch
   cbNewKey_({}, newKey.toHexStr(), srvAddrPort, serverPubkeyProm_);
   bool cbResult = false;

   //have we seen the server's pubkey?
   if (serverPubkeyProm_ != nullptr) {
      //if so, wait on the promise
      cbResult = serverPubkeyProm_->waitValue();
      serverPubkeyProm_.reset();
   }

   if (!cbResult) {
      logger_->info("[{}] User refused new server {} identity key {} - connection refused"
         , __func__, srvAddrPort, newKey.toHexStr());
      return false;
   }

   logger_->info("[{}] Server at {} has new identity key {}. Connection accepted."
      , __func__, srvAddrPort, newKey.toHexStr());
   return true;
}

// Get the signer's identity public key. Intended for use with local signers.
//
// INPUT:  The accompanying key IP:Port or name. (const string)
// OUTPUT: The buffer that will hold the compressed ID key. (BinaryData)
// RETURN: True if success, false if failure.
bool ZmqBIP15XDataConnection::getServerIDCookie(BinaryData& cookieBuf)
{
   if (params_.cookie != BIP15XCookie::ReadServer) {
      return false;
   }

   if (!SystemFileUtils::fileExist(params_.cookiePath)) {
      logger_->error("[{}] Server identity cookie ({}) doesn't exist. Unable "
         "to verify server identity.", __func__, params_.cookiePath);
      return false;
   }

   // Ensure that we only read a compressed key.
   ifstream cookieFile(params_.cookiePath, ios::in | ios::binary);
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
   if (params_.cookie != BIP15XCookie::MakeClient) {
      logger_->error("[{}] ID cookie creation requested but not allowed."
      , __func__);
      return false;
   }

   if (SystemFileUtils::fileExist(params_.cookiePath)) {
      if (!SystemFileUtils::rmFile(params_.cookiePath)) {
         logger_->error("[{}] Unable to delete client identity cookie ({}). "
            "Will not write a new cookie.", __func__, params_.cookiePath);
         return false;
      }
   }

   // Ensure that we only write the compressed key.
   ofstream cookieFile(params_.cookiePath, ios::out | ios::binary);
   const BinaryData ourIDKey = getOwnPubKey();
   if (ourIDKey.getSize() != BTC_ECKEY_COMPRESSED_LENGTH) {
      logger_->error("[{}] Client identity key ({}) is uncompressed. Will not "
         "write the identity cookie.", __func__, params_.cookiePath);
      return false;
   }

   logger_->debug("[{}] Writing a new client identity cookie ({}).", __func__
      ,  params_.cookiePath);
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
   std::lock_guard<std::mutex> lock(authPeersMutex_);
   return getOwnPubKey(*authPeers_);
}


void ZmqBIP15XDataConnection::sendCommand(ZmqBIP15XDataConnection::InternalCommandCode command)
{
   int result = zmq_send(threadMasterSocket_.get(), &command, sizeof(command), 0);
   assert(result == int(sizeof(command)));
}

void ZmqBIP15XDataConnection::sendPendingData()
{
   if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
      return;
   }

   std::vector<std::string> pendingDataTmp;
   {
      std::lock_guard<std::mutex> lock(pendingDataMutex_);
      pendingDataTmp = std::move(pendingData_);
   }

   for (const std::string &data : pendingDataTmp) {
      // If we need to rekey, do it before encrypting the data.
      rekeyIfNeeded(data.size());

      auto connPtr = bip151HandshakeCompleted_ ? bip151Connection_.get() : nullptr;
      auto packet = ZmqBipMsgBuilder(data, ZMQ_MSGTYPE_SINGLEPACKET)
         .encryptIfNeeded(connPtr).build();
      sendPacket(packet);
   }
}

void ZmqBIP15XDataConnection::sendDisconnectMsg()
{
   if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
      return;
   }

   auto packet = ZmqBipMsgBuilder(ZMQ_MSGTYPE_DISCONNECT)
      .encryptIfNeeded(bip151Connection_.get()).build();
   // An error message is already logged elsewhere if the send fails.
   sendPacket(packet);

   if (isConnected_) {
      onDisconnected();
   }
}
