#include <chrono>
#include <QStandardPaths>

#include "ZMQ_BIP15X_ServerConnection.h"
#include "MessageHolder.h"
#include "ZMQ_BIP15X_Msg.h"
#include "ZMQHelperFunctions.h"

using namespace std;

// DESIGN NOTE: The BIP151Connection objects need to be attached to specific
// connections, and they need to be set up and torn down as clients connect and
// disconnect. Due to ZMQ peculiarities, this is more difficult than it should
// be. The data socket doesn't supply any external information. So, a client ID
// from a MessageHolder object is ideal. It's derived from a monitor socket,
// which is accurate in knowing when a client has connected or disconnected.
// Unfortunately, there doesn't seem to be a good way to get the client ID when
// getting a data packet. The only solution that seems to work for now is to get
// the client IP addresses associated with the connections and work off that.
// This isn't ideal - the monitor sockets don't give the port, which means
// multiple connections behind the same IP address require a workaround - but
// this is a start until a better solution can be devised. Ideally,
// OnClientConnected() could potentially be triggered in the listener, which
// could then pass the ID back down here via a callback and into clientInfo_. As
// is, the code takes a similar but different tack by associating the IP address
// with the BIP151Connection object (socketConnMap_).

// The constructor to use.
//
// INPUT:  Logger object. (const shared_ptr<spdlog::logger>&)
//         ZMQ context. (const std::shared_ptr<ZmqContext>&)
//         List of trusted clients. (const QStringList&)
//         Per-connection ID. (const uint64_t&)
//         Ephemeral peer usage. Not recommended. (const bool&)
// OUTPUT: None
zmqBIP15XServerConnection::zmqBIP15XServerConnection(
   const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ZmqContext>& context
   , const QStringList& trustedClients, const uint64_t& id
   , const bool& ephemeralPeers)
   : ZmqServerConnection(logger, context), id_(id)
   , trustedClients_(trustedClients) {
   string datadir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
   string filename(SERVER_AUTH_PEER_FILENAME);

   // In general, load the client key from a special Armory wallet file.
   if (!ephemeralPeers) {
       authPeers_ = make_shared<AuthorizedPeers>(datadir, filename);
   }
   else {
      authPeers_ = make_shared<AuthorizedPeers>();
   }

   // Set up the callbacks for when clients connect and disconnect. For now,
   // disable due to client ID consistency issues.
/*   const auto cbConnected = [this](const std::string& clientID)->void {
      setBIP151Connection(clientID);
   };
   const auto cbDisconnected = [this](const std::string& clientID)->void {
      resetBIP151Connection(clientID);
   };
   setConnAcceptedCB(cbConnected);
   setConnClosedCB(cbDisconnected);*/
}

// Create the data socket.
//
// INPUT:  None
// OUTPUT: None
// RETURN: The data socket. (ZmqContext::sock_ptr)
ZmqContext::sock_ptr zmqBIP15XServerConnection::CreateDataSocket() {
   return context_->CreateServerSocket();
}

// Get the incoming data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XServerConnection::ReadFromDataSocket() {
   MessageHolder clientId;
   MessageHolder data;

   // The client ID will be sent before the actual data.
   int result = zmq_msg_recv(&clientId, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[{}] {} failed to recv header: {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   // Check if we've recorded the client ID.
   const auto &clientIdStr = clientId.ToString();
   if (clientInfo_.find(clientIdStr) == clientInfo_.end()) {
#ifdef WIN32
      SOCKET socket = 0;
#else
      int socket = 0;
#endif
      size_t sockSize = sizeof(socket);
      if (zmq_getsockopt(dataSocket_.get(), ZMQ_FD, &socket, &sockSize) == 0) {
         //
      }
   }

   // Finally, we can grab the incoming data.
   result = zmq_msg_recv(&data, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[{}] {} failed to recv message data: {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }
   if (!data.IsLast()) {
      logger_->error("[{}] {} broken protocol", __func__, connectionName_);
      return false;
   }
   // Process the incoming data.
   ProcessIncomingData(data.ToString(), clientIdStr);

   return true;
}

// The send function for the data connection. Ideally, this should not be used
// before the handshake is completed, but it is possible to use at any time.
// Whether or not the raw data is used, it will be placed in a zmqBIP15XMsg
// object.
//
// INPUT:  The data to send. It'll be encrypted here if needed. (const string&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XServerConnection::SendDataToClient(const string& clientId
   , const string& data, const SendResultCb &cb) {
   string sendStr = data;

   // Encrypt data here if the BIP 150 handshake is complete.
   if (socketConnMap_[clientId]->getBIP150State() == BIP150State::SUCCESS) {
      zmqBIP15XMsg msg;
      BIP151Connection* connPtr = nullptr;
      if (bip151HandshakeCompleted_) {
         connPtr = socketConnMap_[clientId].get();
      }
      BinaryData payload(data);
      vector<BinaryData> outData = msg.serialize(payload.getDataVector()
         , connPtr, ZMQ_MSGTYPE_SINGLEPACKET, 0);
      sendStr = outData[0].toBinStr();
   }

   // Queue up the data for transmission.
   return QueueDataToSend(clientId, sendStr, cb, false);
}

// The function that processes raw ZMQ connection data. It processes the BIP
// 150/151 handshake (if necessary) and decrypts the raw data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void zmqBIP15XServerConnection::ProcessIncomingData(const string& encData
   , const string& clientID) {
   size_t position;

   // Backstop in case the callbacks haven't been used.
   if (socketConnMap_[clientID] == nullptr) {
      setBIP151Connection(clientID);
   }

   // Process all incoming data.
   BinaryData packetData(encData);
   if (packetData.getSize() == 0) {
      logger_->error("[{}] Empty data packet ({}).", __func__, connectionName_);
      return;
   }

   // Decrypt only if the BIP 151 handshake is complete.
   if (bip151HandshakeCompleted_) {
      size_t plainTextSize = packetData.getSize() - POLY1305MACLEN;
      auto result = socketConnMap_[clientID]->decryptPacket(packetData.getPtr()
         , packetData.getSize(), (uint8_t*)packetData.getPtr()
         , packetData.getSize());

      // Did decryption succeed? If packets ever have to be split up, we may
      // have to make like Armory's WS implementation and look for fragments.
      if (result != 0) {
         logger_->error("[{}] - Failed decryption - Result = {}", __func__
            , result);
         return;
      }

      // After decryption, the Poly1305 MAC has been removed.
      packetData.resize(plainTextSize);
   }

   // If the BIP 150/151 handshake isn't complete, take the next handshake step.
   uint8_t msgType =
      zmqBIP15XMsg::getPacketType(packetData.getRef());
   if (msgType > ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      processAEADHandshake(move(packetData), clientID);
      return;
   }

   // We shouldn't get here but just in case....
   if (socketConnMap_[clientID]->getBIP150State() != BIP150State::SUCCESS) {
      //can't get this far without fully setup AEAD
      return;
   }

   // Parse the incoming message.
   zmqBIP15XMsg msgObj;
   msgObj.parsePacket(packetData.getRef());
   if (msgObj.getType() != ZMQ_MSGTYPE_SINGLEPACKET) {
      logger_->error("[{}] - Failed packet parsing", __func__);
      return;
   }

   auto&& outMsg = msgObj.getSingleBinaryMessage();
   if (outMsg.getSize() == 0) {
      logger_->error("[{}] - Incoming packet is empty", __func__);
      return;
   }

   // Pass the final data up the chain.
   notifyListenerOnData(clientID, outMsg.toBinStr());
}

// Configure the data socket.
//
// INPUT:  The data socket to configure. (const ZmqContext::sock_ptr&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XServerConnection::ConfigDataSocket(
   const ZmqContext::sock_ptr& dataSocket) {
   return ZmqServerConnection::ConfigDataSocket(dataSocket);
}

// The function processing the BIP 150/151 handshake packets.
//
// INPUT:  The raw handshake packet data. (const BinaryData&)
//         The client ID. (const string&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool zmqBIP15XServerConnection::processAEADHandshake(const BinaryData& msgObj
   , const string& clientID) {
   // Function used to actually send data to the client.
   auto writeToClient = [this, clientID](uint8_t type, const BinaryDataRef& msg
      , bool encrypt)->bool {
      zmqBIP15XMsg outMsg;
      BIP151Connection* connPtr = nullptr;
      if (encrypt) {
         connPtr = socketConnMap_[clientID].get();
      }

      vector<BinaryData> outData = outMsg.serialize(msg, connPtr, type, 0);
      return SendDataToClient(clientID, move(outData[0].toBinStr()));
   };

   // Handshake function. Code mostly copied from Armory.
   auto processHandshake = [this, &writeToClient, clientID](
      const BinaryData& msgdata)->bool {
      // Parse the packet.
      zmqBIP15XMsg zmqMsg;

      if (!zmqMsg.parsePacket(msgdata.getRef())) {
         //invalid packet
         logger_->error("[processHandshake] BIP 150/151 handshake process "
            "failed - ZMQ packet not properly parsed");
         return false;
      }

      auto dataBdr = zmqMsg.getSingleBinaryMessage();
      switch (zmqMsg.getType()) {
      case ZMQ_MSGTYPE_AEAD_SETUP:
      {
         //send pubkey message
         if (!writeToClient(ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY,
            socketConnMap_[clientID]->getOwnPubKey(), false)) {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AEAD_SETUP: "
               "Response 1 not sent");
         }

         //init bip151 handshake
         BinaryData encinitData(ENCINITMSGSIZE);
         if (socketConnMap_[clientID]->getEncinitData(encinitData.getPtr()
            , ENCINITMSGSIZE
            , BIP151SymCiphers::CHACHA20POLY1305_OPENSSH) != 0) {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCINIT data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AEAD_ENCINIT, encinitData.getRef()
            , false)) {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AEAD_SETUP: "
               "Response 2 not sent");
         }
         break;
      }

      case ZMQ_MSGTYPE_AEAD_ENCACK:
      {
         //process client encack
         if (socketConnMap_[clientID]->processEncack(dataBdr.getPtr()
            , dataBdr.getSize(), true) != 0) {
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
         if (socketConnMap_[clientID]->getBIP150State() != BIP150State::SUCCESS) {
            //can't rekey before auth, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - Not yet able to process a rekey");
            return false;
         }

         // If connection is already set up, we only accept rekey encack messages.
         if (socketConnMap_[clientID]->processEncack(dataBdr.getPtr()
            , dataBdr.getSize(), false) != 0) {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCACK not processed");
            return false;
         }

         break;
      }

      case ZMQ_MSGTYPE_AEAD_ENCINIT:
      {
         //process client encinit
         if (socketConnMap_[clientID]->processEncinit(dataBdr.getPtr()
            , dataBdr.getSize(), false) != 0) {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_ENCINIT processing failed");
            return false;
         }

         //return encack
         BinaryData encackData(BIP151PUBKEYSIZE);
         if (socketConnMap_[clientID]->getEncackData(encackData.getPtr()
            , BIP151PUBKEYSIZE) != 0) {
            //failed to init handshake, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_ENCACK data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AEAD_ENCACK, encackData.getRef()
            , false)) {
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AEAD_ENCACK not sent");
         }

         bip151HandshakeCompleted_ = true;
         break;
      }

      case ZMQ_MSGTYPE_AUTH_CHALLENGE:
      {
         bool goodChallenge = true;
         auto challengeResult =
            socketConnMap_[clientID]->processAuthchallenge(dataBdr.getPtr()
            , dataBdr.getSize(), true); //true: step #1 of 6

         if (challengeResult == -1) {
            //auth fail, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_CHALLENGE processing failed");
            return false;
         }
         else if (challengeResult == 1) {
            goodChallenge = false;
         }

         BinaryData authreplyBuf(BIP151PRVKEYSIZE * 2);
         if (socketConnMap_[clientID]->getAuthreplyData(authreplyBuf.getPtr()
            , authreplyBuf.getSize(), true //true: step #2 of 6
            , goodChallenge) == -1) {
            //auth setup failure, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_REPLY data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AUTH_REPLY, authreplyBuf.getRef()
            , true)) {
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_REPLY not sent");
            return false;
         }

         break;
      }

      case ZMQ_MSGTYPE_AUTH_PROPOSE:
      {
         bool goodPropose = true;
         auto proposeResult = socketConnMap_[clientID]->processAuthpropose(
            dataBdr.getPtr(), dataBdr.getSize());

         if (proposeResult == -1) {
            //auth setup failure, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_PROPOSE processing failed");
            return false;
         }
         else if (proposeResult == 1) {
            goodPropose = false;
         }
         else {
            //keep track of the propose check state
            socketConnMap_[clientID]->setGoodPropose();
         }

         BinaryData authchallengeBuf(BIP151PRVKEYSIZE);
         if (socketConnMap_[clientID]->getAuthchallengeData(authchallengeBuf.getPtr()
            , authchallengeBuf.getSize()
            , "" //empty string, use chosen key from processing auth propose
            , false //false: step #4 of 6
            , goodPropose) == -1) {
            //auth setup failure, kill connection
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_CHALLENGE data not obtained");
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AUTH_CHALLENGE
            , authchallengeBuf.getRef(), true)) {
            logger_->error("[processHandshake] BIP 150/151 handshake process "
               "failed - AUTH_CHALLENGE not sent");
         }

         break;
      }

      case ZMQ_MSGTYPE_AUTH_REPLY:
      {
         if (socketConnMap_[clientID]->processAuthreply(dataBdr.getPtr()
            , dataBdr.getSize(), false
            , socketConnMap_[clientID]->getProposeFlag()) != 0) {
            //invalid auth setup, kill connection
            return false;
         }

         //rekey after succesful BIP150 handshake
         socketConnMap_[clientID]->bip150HandshakeRekey();
         bip150HandshakeCompleted_ = true;
         outKeyTimePoint_ = chrono::system_clock::now();

         break;
      }

      default:
         logger_->error("[processHandshake] Unknown message type.");
         return false;
      }

      return true;
   };

   bool retVal = processHandshake(msgObj);
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
void zmqBIP15XServerConnection::resetBIP151Connection(
   const string& clientID) {
   if (socketConnMap_[clientID] != nullptr) {
      socketConnMap_[clientID].reset();
   }
   else {
      BinaryData hexID(clientID);
      logger_->error("[{}] Client ID {} does not exist.", __func__
         , hexID.toHexStr());
   }
}

// Function used to set the BIP 150/151 handshake data. Called when a connection
// is created.
// 
// INPUT:  The client ID. (const string&)
// OUTPUT: None
// RETURN: None
void zmqBIP15XServerConnection::setBIP151Connection(
   const string& clientID) {
   if (socketConnMap_[clientID] == nullptr) {
      auto lbds = getAuthPeerLambda();
      for (auto b : trustedClients_) {
         QStringList nameKeyList = b.split(QStringLiteral(":"));
/*            if (nameKeyList.size() != 2) {
               return -1;
            }*/
         vector<string> keyName;
         keyName.push_back(nameKeyList[0].toStdString());
         SecureBinaryData inKey = READHEX(nameKeyList[1].toStdString());
         authPeers_->addPeer(inKey, keyName);
      }
      socketConnMap_[clientID] = make_unique<BIP151Connection>(lbds);
   }
   else {
      BinaryData hexID(clientID);
      logger_->error("[{}] Client ID {} already exists.", __func__
         , hexID.toHexStr());
   }
}

// Get lambda functions related to authorized peers. Copied from Armory.
//
// INPUT:  None
// OUTPUT: None
// RETURN: AuthPeersLambdas object with required lambdas.
AuthPeersLambdas zmqBIP15XServerConnection::getAuthPeerLambda() {
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
