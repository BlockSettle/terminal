#include <chrono>
#include <QStandardPaths>

#include "ZMQ_BIP15X_ServerConnection.h"
#include "MessageHolder.h"
#include "ZMQ_BIP15X_Msg.h"
#include "ZMQHelperFunctions.h"

using namespace std;

ZMQ_BIP15X_ServerConnection::ZMQ_BIP15X_ServerConnection(
   const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ZmqContext>& context
   , const QStringList& trustedClients, const uint64_t& id
   , const bool& ephemeralPeers)
   : ZmqServerConnection(logger, context), id_(id) {
      string datadir =
         QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
      string filename(SERVER_AUTH_PEER_FILENAME);
      if (!ephemeralPeers) {
          authPeers_ = make_shared<AuthorizedPeers>(datadir, filename);
      }
      else {
         authPeers_ = make_shared<AuthorizedPeers>();
      }

      auto lbds = getAuthPeerLambda();
      for (auto b : trustedClients) {
         QStringList nameKeyList = b.split(QStringLiteral(":"));
/*         if (nameKeyList.size() != 2) {
            return -1;
         }*/
         vector<string> keyName;
         keyName.push_back(nameKeyList[0].toStdString());
         SecureBinaryData inKey = READHEX(nameKeyList[1].toStdString());
         authPeers_->addPeer(inKey, keyName);
      }
      bip151Connection_ = make_shared<BIP151Connection>(lbds);

/*      writeLock_ = std::make_shared<std::atomic<unsigned>>();
      writeLock_->store(0);

      readLock_ = std::make_shared<std::atomic<unsigned>>();
      readLock_->store(0);

      serializedStack_ = std::make_shared<Queue<SerializedMessage>>();
      readQueue_ = std::make_shared<Queue<BinaryData>>();

      count_ = std::make_shared<std::atomic<int>>();
      count_->store(0, std::memory_order_relaxed);

      run_ = std::make_shared<std::atomic<int>>();
      run_->store(0, std::memory_order_relaxed);*/
}

ZmqContext::sock_ptr ZMQ_BIP15X_ServerConnection::CreateDataSocket() {
   return context_->CreateServerSocket();
}

bool ZMQ_BIP15X_ServerConnection::ReadFromDataSocket() {
   MessageHolder clientId;
   MessageHolder data;

   int result = zmq_msg_recv(&clientId, dataSocket_.get(), ZMQ_DONTWAIT);
   if (result == -1) {
      logger_->error("[{}] {} failed to recv header: {}", __func__
         , connectionName_, zmq_strerror(zmq_errno()));
      return false;
   }

   const auto &clientIdStr = clientId.ToString();
   if (clientInfo_.find(clientIdStr) == clientInfo_.end()) {
#ifdef WIN32
      SOCKET socket = 0;
#else
      int socket = 0;
#endif
      size_t sockSize = sizeof(socket);
      if (zmq_getsockopt(dataSocket_.get(), ZMQ_FD, &socket, &sockSize) == 0) {
         clientInfo_[clientIdStr] = bs::network::peerAddressString(static_cast<int>(socket));
      }
   }

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

   // Decrypt data here.
   ProcessIncomingData(data.ToString(), clientIdStr);
//   if (decData == "") {
//      logger_->error("[{}] {} failed decryption", __func__, connectionName_);
//      return false;
//   }

// TO DO: Refactor such that decrypted data is placed in notification
//   notifyListenerOnData(clientIdStr, data.ToString());
   return true;
}

//
bool ZMQ_BIP15X_ServerConnection::SendDataToClient(const string& clientId
   , const string& data, const SendResultCb &cb) {
   // TO DO: Encrypt data here if needed.

   return QueueDataToSend(clientId, data, cb, false);
}


void ZMQ_BIP15X_ServerConnection::ProcessIncomingData(const string& encData
   , const string& clientID) {
   size_t position;

   // Process all incoming data.
//   string message = pendingData_;
   BinaryData packetData(encData);

/*   try
   {
      packetData = move(readQueue_->pop_front());
   }
   catch (IsEmpty&)
   {
      //end loop condition
      return;
   }*/

   if (packetData.getSize() == 0) {
//      LOGWARN << "empty command packet";
      return;
   }

/*   if (readLeftOverData_.getSize() != 0)
   {
      readLeftOverData_.append(packetData);
      packetData = move(readLeftOverData_);
      readLeftOverData_.clear();
   }*/

   if (bip151HandshakeCompleted_) {
      //decrypt packet
      size_t plainTextSize = packetData.getSize() - POLY1305MACLEN;
      auto result = bip151Connection_->decryptPacket(packetData.getPtr()
         , packetData.getSize(), (uint8_t*)packetData.getPtr()
         , packetData.getSize());

      if (result != 0) {
//         if (result <= ZMQ_MESSAGE_PACKET_SIZE && result > -1)
//         {
            /*
            lws receives packet in the order the counterpart sent them, but
            it may break down a packet into several payloads, dependent on the
            write buffer fillrate.

            The AEAD layer requires full packets to verify the attached MAC,
            meaning we cannot distinguish between packets with invalid encryption
            and partially transmitted packets with valid encryption until we have
            as many bytes as the advertized chacha20 size available to us.

            At same time we can reject packets that advertize a size superior to
            our expected maximum packet size (WEBSOCKET_MESSAGE_PACKET_SIZE),
            which is often the case when deciphering the length of an invalidly
            encrypted packet.

            Since lws does not spill packets onto one another, there is no risk
            that the data we receive carries the head of another packet at its tail.
            Reconstruction is therefor a simple case of appending the incoming data
            to the previous left over until we have enough data to decrypt for the
            advertized packet size.
            */
//            readLeftOverData_ = move(packetData);
//            return;
         }

// TO DO: Double check logic.
      packetData.resize(plainTextSize);
   }

   uint8_t msgType =
      ZMQ_BIP15X_Msg::getPacketType(packetData.getRef());

   if (msgType > ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      processAEADHandshake(move(packetData), clientID);
      return;
   }

   if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
      //can't get this far without fully setup AEAD
      return;
   }

//   BinaryDataRef bdr((uint8_t*)&id_, 8);
//   auto&& hexID = bdr.toHexStr();
//   auto bdvPtr = clients->get(hexID);

/*   if (bdvPtr != nullptr)
   {
      //create payload
      auto bdv_payload = make_shared<BDV_Payload>();
      bdv_payload->bdvPtr_ = bdvPtr;
      bdv_payload->packetData_ = move(packetData);
      bdv_payload->bdvID_ = id_;

      //queue for clients thread pool to process
      clients->queuePayload(bdv_payload);
   }
   else
   {*/
      //unregistered command
      ZMQ_BIP15X_Msg msgObj;
      msgObj.parsePacket(packetData.getRef());
      if (msgObj.getType() != ZMQ_MSGTYPE_SINGLEPACKET) {
         //invalid msg type, kill connection
         return;
      }

      auto&& messageRef = msgObj.getSingleBinaryMessage();

      if (messageRef.getSize() == 0) {
         //invalid msg, kill connection
         return;
      }

      //process command
/*      auto message = make_shared<::Codec_BDVCommand::StaticCommand>();
      if (!message->ParseFromArray(messageRef.getPtr(), messageRef.getSize()))
      {
         //invalid msg, kill connection
         continue;
      }

      auto&& reply = clients->processUnregisteredCommand(
         id_, message);*/

      //reply
// TO DO: Proper send call.
//      SendDataToClient(id_, msgObj.getId(), reply);
//   }
}

bool ZMQ_BIP15X_ServerConnection::ConfigDataSocket(
   const ZmqContext::sock_ptr& dataSocket) {
   return ZmqServerConnection::ConfigDataSocket(dataSocket);
}

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_ServerConnection::processAEADHandshake(const BinaryData& msgObj
   , const string& clientID) {
   auto writeToClient = [this, clientID](uint8_t type, const BinaryDataRef& msg
      , bool encrypt)->bool {
      ZMQ_BIP15X_Msg outMsg;
      BIP151Connection* connPtr = nullptr;
      if (encrypt) {
         connPtr = bip151Connection_.get();
      }

      vector<BinaryData> outData = outMsg.serialize(msg, connPtr, type, 0);
//      serializedStack_->push_back(move(aeadMsg));

      return SendDataToClient(clientID, move(outData[0].toBinStr()));
   };

   auto processHandshake = [this, &writeToClient](
      const BinaryData& msgdata)->bool {
      ZMQ_BIP15X_Msg zmqMsg;

      if (!zmqMsg.parsePacket(msgdata.getRef())) {
         //invalid packet
         return false;
      }

      auto dataBdr = zmqMsg.getSingleBinaryMessage();
      switch (zmqMsg.getType()) {
      case ZMQ_MSGTYPE_AEAD_SETUP:
      {
         //send pubkey message
         if (!writeToClient(ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY,
            bip151Connection_->getOwnPubKey(), false)) {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AEAD_SETUP: "
               "Response 1 not sent");
         }

         //init bip151 handshake
         BinaryData encinitData(ENCINITMSGSIZE);
         if (bip151Connection_->getEncinitData(encinitData.getPtr()
            , ENCINITMSGSIZE
            , BIP151SymCiphers::CHACHA20POLY1305_OPENSSH) != 0) {
            //failed to init handshake, kill connection
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
         if (bip151Connection_->processEncack(dataBdr.getPtr()
            , dataBdr.getSize(), true) != 0) {
            //failed to init handshake, kill connection
            return false;
         }

         break;
      }

      case ZMQ_MSGTYPE_AEAD_REKEY:
      {
         if (bip151Connection_->getBIP150State() != BIP150State::SUCCESS) {
            //can't rekey before auth, kill connection
            return false;
         }

         //process rekey
         if (bip151Connection_->processEncack(dataBdr.getPtr()
            , dataBdr.getSize(), false) != 0) {
            //failed to init handshake, kill connection
            std::cout << "failed to process rekey" << std::endl;
            return false;
         }

         break;
      }

      case ZMQ_MSGTYPE_AEAD_ENCINIT:
      {
         //process client encinit
         if (bip151Connection_->processEncinit(dataBdr.getPtr()
            , dataBdr.getSize(), false) != 0) {
            //failed to init handshake, kill connection
            return false;
         }

         //return encack
         BinaryData encackData(BIP151PUBKEYSIZE);
         if (bip151Connection_->getEncackData(encackData.getPtr()
            , BIP151PUBKEYSIZE) != 0) {
            //failed to init handshake, kill connection
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AEAD_ENCACK, encackData.getRef()
            , false)) {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AEAD_ENCINIT: "
               "Response not sent");
         }

         bip151HandshakeCompleted_ = true;
         break;
      }

      case ZMQ_MSGTYPE_AUTH_CHALLENGE:
      {
         bool goodChallenge = true;
         auto challengeResult =
            bip151Connection_->processAuthchallenge(dataBdr.getPtr()
            , dataBdr.getSize(), true); //true: step #1 of 6

         if (challengeResult == -1) {
            //auth fail, kill connection
            return false;
         }
         else if (challengeResult == 1) {
            goodChallenge = false;
         }

         BinaryData authreplyBuf(BIP151PRVKEYSIZE * 2);
         if (bip151Connection_->getAuthreplyData(authreplyBuf.getPtr()
            , authreplyBuf.getSize(), true //true: step #2 of 6
            , goodChallenge) == -1) {
            //auth setup failure, kill connection
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AUTH_REPLY, authreplyBuf.getRef()
            , true)) {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AUTH_CHALLENGE: "
               "Response not sent");
         }

         break;
      }

      case ZMQ_MSGTYPE_AUTH_PROPOSE:
      {
         bool goodPropose = true;
         auto proposeResult = bip151Connection_->processAuthpropose(
            dataBdr.getPtr(), dataBdr.getSize());

         if (proposeResult == -1) {
            //auth setup failure, kill connection
            return false;
         }
         else if (proposeResult == 1) {
            goodPropose = false;
         }
         else {
            //keep track of the propose check state
            bip151Connection_->setGoodPropose();
         }

         BinaryData authchallengeBuf(BIP151PRVKEYSIZE);
         if (bip151Connection_->getAuthchallengeData(authchallengeBuf.getPtr()
            , authchallengeBuf.getSize()
            , "" //empty string, use chosen key from processing auth propose
            , false //false: step #4 of 6
            , goodPropose) == -1) {
            //auth setup failure, kill connection
            return false;
         }

         if (!writeToClient(ZMQ_MSGTYPE_AUTH_CHALLENGE
            , authchallengeBuf.getRef(), true)) {
            logger_->error("[processHandshake] ZMG_MSGTYPE_AUTH_PROPOSE: "
               "Response not sent");
         }

         break;
      }

      case ZMQ_MSGTYPE_AUTH_REPLY:
      {
         if (bip151Connection_->processAuthreply(dataBdr.getPtr()
            , dataBdr.getSize(), false
            , bip151Connection_->getProposeFlag()) != 0) {
            //invalid auth setup, kill connection
            return false;
         }

         //rekey after succesful BIP150 handshake
         bip151Connection_->bip150HandshakeRekey();
         outKeyTimePoint_ = chrono::system_clock::now();

         break;
      }

      default:
         logger_->error("[processHandshake] Unknown message type.");
         //unexpected msg id, kill connection
         return false;
      }

      return true;
   };

   if (!processHandshake(msgObj)) {
      logger_->error("[{}] BIP 150/151 handshake process failed.");
   }
}

// Copied from Armory.
AuthPeersLambdas ZMQ_BIP15X_ServerConnection::getAuthPeerLambda() const {
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

///////////////////////////////////////////////////////////////////////////////
/*void ZMQ_BIP15X_ServerConnection::closeConnection() {
   run_->store(-1, memory_order_relaxed);
}*/
