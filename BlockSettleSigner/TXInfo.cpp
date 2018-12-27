#include "TXInfo.h"
#include "HDWallet.h"
#include "QWalletInfo.h"
#include "WalletsManager.h"

using namespace bs::hd;


TXInfo::TXInfo(const std::shared_ptr<WalletsManager> &walletsMgr, const bs::wallet::TXSignRequest &txReq)
   : QObject(), walletsMgr_(walletsMgr), txReq_(txReq)
{
   init();
}

void TXInfo::init()
{
   if (txReq_.wallet) {
      const auto &rootWallet = walletsMgr_->GetHDRootForLeaf(txReq_.wallet->GetWalletId());
      if (rootWallet) {
         walletInfo_ = new bs::hd::WalletInfo(this);
         walletInfo_->initFromWallet(txReq_.wallet, rootWallet->getWalletId());
      }
      else {
         throw std::runtime_error("no root wallet for leaf " + txReq_.wallet->GetWalletId());
      }
   }
   else {
      walletInfo_ = new WalletInfo(walletsMgr_, QString::fromStdString(txReq_.walletId), this);
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

double TXInfo::inputAmount() const
{
   uint64_t result = 0;
   for (const auto &utxo: txReq_.inputs) {
      result += utxo.getValue();
   }
   return result / BTCNumericTypes::BalanceDivider;
}
