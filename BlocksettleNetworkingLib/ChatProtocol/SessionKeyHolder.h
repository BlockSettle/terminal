#ifndef SessionKeyHolder_h__
#define SessionKeyHolder_h__

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
      SessionKeyHolder(const LoggerPtr& loggerPtr, QObject* parent = nullptr);

   public slots:
      void requestSessionKeysForUser(const std::string& userName, const BinaryData& remotePublicKey);
      void onIncomingRequestSessionKeyExchange(const std::string& userName, const BinaryData& incomingEncodedPublicKey, const SecureBinaryData& ownPrivateKey);
      void onIncomingReplySessionKeyExchange(const std::string& userName, const BinaryData& incomingEncodedPublicKey);

   signals:
      void sessionKeysForUser(const Chat::SessionKeyDataPtr& sessionKeyDataPtr);
      void error(const Chat::SessionKeyHolderError& error, const std::string& what);
      void requestSessionKeyExchange(const std::string& userName, const BinaryData& encodedLocalSessionPublicKey);
      void replySessionKeyExchange(const std::string& userName, const BinaryData& encodedLocalSessionPublicKey);

   private slots:
      void handleLocalErrors(const Chat::SessionKeyHolderError& errorCode, const std::string& what = "");

   private:
      BinaryData iesEncryptLocalSessionPublicKey(const Chat::SessionKeyDataPtr& sessionKeyDataPtr, const BinaryData& remotePublicKey) const;
      BinaryData iesDecryptData(const BinaryData& encodedData, const SecureBinaryData& privateKey);
      void generateLocalKeys(const Chat::SessionKeyDataPtr& sessionKeyDataPtr);
      SessionKeyDataPtr sessionDataKeyForUser(const std::string& userName);

      LoggerPtr loggerPtr_;
      std::unordered_map<std::string, SessionKeyDataPtr> sessionKeyDataList_;
   };

   using SessionKeyHolderPtr = std::shared_ptr<SessionKeyHolder>;
}

#endif // SessionKeyHolder_h__
