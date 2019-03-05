#include "TXInfo.h"
#include "HDWallet.h"
#include "QWalletInfo.h"
#include "QmlFactory.h"
#include "WalletsManager.h"

using namespace bs::hd;
using namespace bs::wallet;

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
