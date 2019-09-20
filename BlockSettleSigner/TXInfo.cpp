#include "TXInfo.h"

#include "CheckRecipSigner.h"
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

bool TXInfo::containsAddressImpl(const bs::Address &address, bs::core::wallet::Type walletType) const
{
   for (const auto &leaf : walletsMgr_->getAllWallets()) {
      if (leaf->type() == walletType && leaf->containsAddress(address)) {
         return true;
      }
   }

   return false;
}

bool TXInfo::notContainsAddressImpl(const bs::Address &address) const
{
   // not equal to !containsAddressImpl()
   bool contains = false;

   for (const auto &leaf : walletsMgr_->getAllWallets()) {
      if (leaf->containsAddress(address)) {
         contains = true;
         break;
      }
   }

   return !contains;
}

void TXInfo::setTxId(const QString &txId)
{
   txId_ = txId;
   emit dataChanged();
}

double TXInfo::amountCCReceived(const QString &cc) const
{
   ContainsAddressCb &containsCCAddressCb = [this, cc](const bs::Address &address){
      const auto &wallet = walletsMgr_->getCCWallet(cc.toStdString());
      return wallet->containsAddress(address);
   };

   return txReq_.amountReceived(containsCCAddressCb) / BTCNumericTypes::BalanceDivider;
}

double TXInfo::amountXBTReceived() const
{
   // calculate received amount from counterparty outputs
   // check all wallets and addresses

   return txReq_.amountReceived(containsAnyOurXbtAddressCb_) / BTCNumericTypes::BalanceDivider;
}

QStringList TXInfo::inputs(bs::core::wallet::Type leafType) const
{
   std::vector<UTXO> inputsList;
   if (leafType == bs::core::wallet::Type::Bitcoin) {
      inputsList = txReq_.getInputs(containsAnyOurXbtAddressCb_);
   }
   else if (leafType == bs::core::wallet::Type::ColorCoin) {
      inputsList = txReq_.getInputs(containsAnyOurCCAddressCb_);
   }

   QStringList result;
   for (const auto &input : inputsList) {
      result.push_back(QString::fromStdString(bs::Address::fromUTXO(input).display()));
   }

   result.removeDuplicates();
   return result;
}

QStringList TXInfo::recipients() const
{
   std::vector<std::shared_ptr<ScriptRecipient>> recipientsList;
   recipientsList = txReq_.getRecipients(containsCounterPartyAddressCb_);

   QStringList result;
   for (const auto &recip : recipientsList) {
      const auto addr = bs::Address::fromRecipient(recip);
      result.push_back(QString::fromStdString(addr.display()));
   }

   result.removeDuplicates();
   return result;
}

QStringList TXInfo::inputsXBT() const
{
   return inputs(bs::core::wallet::Type::Bitcoin);
}

QStringList TXInfo::inputsCC() const
{
   return inputs(bs::core::wallet::Type::ColorCoin);
}
