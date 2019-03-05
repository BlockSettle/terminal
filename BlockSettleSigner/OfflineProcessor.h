#ifndef __OFFLINE_PROCESSOR_H__
#define __OFFLINE_PROCESSOR_H__

#include <functional>
#include <memory>
#include <QObject>
#include "EncryptionUtils.h"
#include "MetaData.h"


namespace spdlog {
   class logger;
}
namespace bs {
   class Wallet;
}
class WalletsManager;


class OfflineProcessor : public QObject
{
   Q_OBJECT

public:
   using cbPassword = std::function<SecureBinaryData(const std::shared_ptr<bs::Wallet> &)>;

   OfflineProcessor(const std::shared_ptr<spdlog::logger> &, const std::shared_ptr<WalletsManager> &
      , const cbPassword &cb = nullptr);

   void ProcessFiles(const QStringList &);
   Q_INVOKABLE void processFile(const QString &file);
   Q_INVOKABLE void processRequest(int reqId);
   Q_INVOKABLE int parseFile(QString file);
   Q_INVOKABLE QString parsedText(int reqId) const;
   Q_INVOKABLE void removeSignReq(int reqId);

signals:
   void requestPassword(const bs::wallet::TXSignRequest &);
   void signSuccess();
   void signFailure();

public slots:
   void passwordEntered(const std::string &walletId, const SecureBinaryData &password);

private:
   struct SignData {
      bs::wallet::TXSignRequest     request;
      std::shared_ptr<bs::Wallet>   wallet;
      QString     requestFile;
   };

   SignData ParseSignTX(const bs::wallet::TXSignRequest &txReq, const QString &reqFN);
   void ProcessSignTX(const bs::wallet::TXSignRequest &txReq, const QString &reqFN);
   void SignTxRequest(const bs::wallet::TXSignRequest &txReq, const QString &reqFN
      , const std::shared_ptr<bs::Wallet> &, const SecureBinaryData &password = {});

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<WalletsManager>  walletsMgr_;
   const cbPassword     cbPassword_;
   std::unordered_map<std::string, std::vector<SignData>>   pendingReqs_;
   std::map<int, std::vector<SignData>>   parsedReqs_;
   int reqSeqNum_ = 0;
};

#endif // __OFFLINE_PROCESSOR_H__
