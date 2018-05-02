#include "TXInfo.h"
#include "HDWallet.h"
#include "WalletsManager.h"


TXInfo::TXInfo(const std::shared_ptr<WalletsManager> &walletsMgr, const bs::wallet::TXSignRequest &txReq)
   : QObject(), walletsMgr_(walletsMgr), txReq_(txReq)
{
   init();
}

void TXInfo::init()
{
   const auto wallet = txReq_.walletId.empty() ? txReq_.wallet : walletsMgr_->GetWalletById(txReq_.walletId).get();
   if (wallet) {
      walletName_ = QString::fromStdString(wallet->GetWalletName());
   }
   else {
      const auto &hdWallet = walletsMgr_->GetHDWalletById(txReq_.walletId);
      if (hdWallet) {
         walletName_ = QString::fromStdString(hdWallet->getName());
      }
   }
   emit dataChanged();
}

QStringList TXInfo::recvAddresses() const
{
   QStringList result;
   for (const auto &recip : txReq_.recipients) {
      result.push_back(bs::Address::fromRecipient(recip).display());
   }
   return result;
}

double TXInfo::amount() const
{
   uint64_t result = 0;
   for (const auto &recip : txReq_.recipients) {
      result += recip->getValue();
   }
   return result / BTCNumericTypes::BalanceDivider;
}
