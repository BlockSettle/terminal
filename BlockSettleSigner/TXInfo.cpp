#include "TXInfo.h"

#include "Wallets/SyncHDWallet.h"
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

double TXInfo::amountCCReceived(const QString &cc) const
{
   const std::function<bool(const bs::Address &)> &containsCCAddressCb = [this, cc](const bs::Address &address){
      const auto &wallet = walletsMgr_->getCCWallet(cc.toStdString());
      return wallet->containsAddress(address);
   };

   return txReq_.amountReceived(containsCCAddressCb) / BTCNumericTypes::BalanceDivider;
}

double TXInfo::amountXBTReceived() const
{
   // calculate received amount from counterparty outputs
   // check all wallets and addresses

   const std::function<bool(const bs::Address &)> &containsXbtAddressCb = [this](const bs::Address &address){
      for (unsigned int i = 0; i < walletsMgr_->hdWalletsCount(); i++) {
         const auto &wallet = walletsMgr_->getHDWallet(i);
         for (auto leaf : wallet->getLeaves()) {
            if (leaf->type() == core::wallet::Type::Bitcoin && leaf->containsAddress(address)) {
               return true;
            }
         }
      }
      return false;
   };

   return txReq_.amountReceived(containsXbtAddressCb) / BTCNumericTypes::BalanceDivider;
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

