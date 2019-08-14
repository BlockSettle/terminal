#include <QtConcurrent/QtConcurrent>
#include <QFuture>

#include "ChatProtocol/CryptManager.h"
#include "Encryption/IES_Encryption.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include "botan/base64.h"
#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <enable_warnings.h>

#include "chat.pb.h"

namespace Chat
{
   CryptManager::CryptManager(const Chat::LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
      : QObject(parent), loggerPtr_(loggerPtr)
   {

   }

   QFuture<PartyMessagePacket> CryptManager::encryptMessageIes(const PartyMessagePacket& partyMessagePacket, const BinaryData& pubKey)
   {
      auto encryptMessageWorker = [this, pubKey](PartyMessagePacket partyMessagePacket) {

         auto cipher = Encryption::IES_Encryption::create(loggerPtr_);

         cipher->setPublicKey(pubKey);
         cipher->setData(partyMessagePacket.message());

         Botan::SecureVector<uint8_t> output;
         cipher->finish(output);

         partyMessagePacket.set_encryption(Chat::EncryptionType::IES);
         partyMessagePacket.set_message(Botan::base64_encode(output));

         return partyMessagePacket;
      };

      return QtConcurrent::run(encryptMessageWorker, partyMessagePacket);
   }
}