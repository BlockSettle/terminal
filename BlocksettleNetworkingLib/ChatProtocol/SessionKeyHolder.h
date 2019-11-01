#ifndef SESSIONKEYHOLDER_H
#define SESSIONKEYHOLDER_H

#include <QObject>
#include <unordered_map>

#include "ChatProtocol/SessionKeyData.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   enum class SessionKeyHolderError
   {
      PublicKeyEmpty,
      IesEncoding,
      IesDecodingSessionKeyRequest,
      IesDecoding
   };

   class SessionKeyHolder : public QObject
   {
      Q_OBJECT
   public:
      SessionKeyHolder(LoggerPtr loggerPtr, QObject* parent = nullptr);

   public slots:
      void requestSessionKeysForUser(const std::string& userName, const BinaryData& remotePublicKey);
      void onIncomingRequestSessionKeyExchange(const std::string& userName, const BinaryData& incomingEncodedPublicKey, const SecureBinaryData& ownPrivateKey);
      void onIncomingReplySessionKeyExchange(const std::string& userName, const BinaryData& incomingEncodedPublicKey);
      SessionKeyDataPtr sessionKeyDataForUser(const std::string& userName);
      void clearSessionForUser(const std::string& userName);

   signals:
      void sessionKeysForUser(const Chat::SessionKeyDataPtr& sessionKeyDataPtr);
      void sessionKeysForUserFailed(const std::string& userName);
      void error(const Chat::SessionKeyHolderError& error, const std::string& what);
      void requestSessionKeyExchange(const std::string& userName, const BinaryData& encodedLocalSessionPublicKey);
      void replySessionKeyExchange(const std::string& userName, const BinaryData& encodedLocalSessionPublicKey);

   private slots:
      void handleLocalErrors(const Chat::SessionKeyHolderError& errorCode, const std::string& what = "") const;

   private:
      BinaryData iesEncryptLocalSessionPublicKey(const Chat::SessionKeyDataPtr& sessionKeyDataPtr, const BinaryData& remotePublicKey) const;
      BinaryData iesDecryptData(const BinaryData& encodedData, const SecureBinaryData& privateKey);
      static void generateLocalKeys(const Chat::SessionKeyDataPtr& sessionKeyDataPtr);

      LoggerPtr loggerPtr_;
      std::unordered_map<std::string, SessionKeyDataPtr> sessionKeyDataList_;
   };

   using SessionKeyHolderPtr = std::shared_ptr<SessionKeyHolder>;
}

#endif // SESSIONKEYHOLDER_H
