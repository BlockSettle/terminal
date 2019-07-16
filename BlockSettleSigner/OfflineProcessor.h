#ifndef __OFFLINE_PROCESSOR_H__
#define __OFFLINE_PROCESSOR_H__

#include <functional>
#include <memory>
#include <QObject>
#include "EncryptionUtils.h"
#include "CoreWallet.h"


namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class Wallet;
   }
}
class SignerAdapter;

class OfflineProcessor : public QObject
{
   Q_OBJECT

public:
   using CbPassword = std::function<SecureBinaryData(const std::shared_ptr<bs::sync::Wallet> &)>;

   OfflineProcessor(const std::shared_ptr<spdlog::logger> &
      , SignerAdapter *, const CbPassword &cb = nullptr);

   void ProcessFiles(const QStringList &);
   Q_INVOKABLE void processFile(const QString &file);
   Q_INVOKABLE void processRequest(int reqId);
   Q_INVOKABLE int parseFile(QString file);
   Q_INVOKABLE QString parsedText(int reqId) const;
   Q_INVOKABLE void removeSignReq(int reqId);

signals:
   void requestPassword(const bs::core::wallet::TXSignRequest &);
   void signSuccess(const QString &filePath);
   void signFailure(const QString &errorMsg);

public slots:
   void passwordEntered(const std::string &walletId, const SecureBinaryData &password, bool cancelledByUser);

private:
   struct SignData {
      bs::core::wallet::TXSignRequest     request;
      std::shared_ptr<bs::sync::Wallet>   wallet;
      QString     requestFile;
   };

   SignData ParseSignTX(const bs::core::wallet::TXSignRequest &txReq, const QString &reqFN);
   void ProcessSignTX(const bs::core::wallet::TXSignRequest &txReq, const QString &reqFN);
   void SignTxRequest(const bs::core::wallet::TXSignRequest &txReq, const QString &reqFN
      , const SecureBinaryData &password = {});
   void sendSignFailure(const QString &errorMsg);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   SignerAdapter     *  adapter_;
   const CbPassword     cbPassword_;
   std::unordered_map<std::string, std::vector<SignData>>   pendingReqs_;
   std::map<int, std::vector<SignData>>   parsedReqs_;
   int reqSeqNum_ = 0;
};

#endif // __OFFLINE_PROCESSOR_H__
