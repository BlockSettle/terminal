#ifndef CryptManager_h__
#define CryptManager_h__

#include <QObject>
#include <QFuture>
#include <memory>

namespace spdlog
{
   class logger;
}

class BinaryData;

namespace Chat
{
   class PartyMessagePacket;

   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class CryptManager : public QObject
   {
      Q_OBJECT
   public:
      CryptManager(const Chat::LoggerPtr& loggerPtr, QObject* parent = nullptr);

      QFuture<PartyMessagePacket> encryptMessageIes(const PartyMessagePacket& partyMessagePacket, const BinaryData& pubKey);
      //static std::shared_ptr<Chat::Data> decryptMessageIes(const Chat::Data_Message& msg, const SecureBinaryData& privKey);

   private:
      LoggerPtr loggerPtr_;
   };

   using CryptManagerPtr = std::shared_ptr<CryptManager>;
}

#endif // CryptManager_h__