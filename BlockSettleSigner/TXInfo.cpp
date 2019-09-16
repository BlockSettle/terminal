#include "TXInfo.h"
#include "QWalletInfo.h"

using namespace bs::wallet;
using namespace Blocksettle::Communication;

TXInfo::TXInfo(const bs::core::wallet::TXSignRequest &txReq, const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<spdlog::logger> &logger)
   : QObject(), txReq_(txReq), walletsMgr_(walletsMgr), logger_(logger)
{
   init();
}

TXInfo::TXInfo(const headless::SignTxRequest &txRequest, const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<spdlog::logger> &logger)
   : QObject(), txReq_(bs::signer::pbTxRequestToCore(txRequest)), walletsMgr_(walletsMgr), logger_(logger)
{
   init();
}

TXInfo::TXInfo(const TXInfo &src)
   : QObject(), txReq_(src.txReq_), walletsMgr_(src.walletsMgr_), logger_(src.logger_)
{
   init();
}

void TXInfo::init()
{
   txId_ = QString::fromStdString(txReq_.serializeState().toBinStr());
}

void TXInfo::setTxId(const QString &txId)
{
   txId_ = txId;
   emit dataChanged();
}

QStringList TXInfo::inputs() const
{
   QStringList result;
   for (const auto &input : txReq_.inputs) {
      result.push_back(QString::fromStdString(bs::Address::fromUTXO(input).display()));
   }
   return result;
}

QStringList TXInfo::recipients() const
{
   QStringList result;
   for (const auto &recip : txReq_.recipients) {
      result.push_back(QString::fromStdString(bs::Address::fromRecipient(recip).display()));
   }
   return result;
}
