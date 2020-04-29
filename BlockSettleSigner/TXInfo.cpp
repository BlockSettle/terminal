/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TXInfo.h"

#include "CheckRecipSigner.h"
#include "OfflineSigner.h"
#include "TxClasses.h"
#include "Wallets/SyncHDWallet.h"
#include "QWalletInfo.h"

#include <QFile>

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
   : QObject(), txReq_(bs::signer::pbTxRequestToCore(txRequest, logger)), walletsMgr_(walletsMgr), logger_(logger)
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

QString TXInfo::walletId() const
{
   if (txReq_.walletIds.empty()) {
      return {};
   }
   return QString::fromStdString(txReq_.walletIds.front());
}

double TXInfo::amountCCReceived(const QString &cc) const
{
   ContainsAddressCb &containsCCAddressCb = [this, cc](const bs::Address &address){
      const auto &wallet = walletsMgr_->getCCWallet(cc.toStdString());
      return wallet->containsAddress(address);
   };

   return txReq_.amountReceived(containsCCAddressCb) / BTCNumericTypes::BalanceDivider;
}

double TXInfo::amountCCSent() const
{
   return txReq_.totalSpent(containsAnyOurCCAddressCb_) / BTCNumericTypes::BalanceDivider;

}

double TXInfo::amountXBTReceived() const
{
   // calculate received amount from counterparty outputs
   // check all wallets and addresses

   return txReq_.amountReceived(containsAnyOurXbtAddressCb_) / BTCNumericTypes::BalanceDivider;
}

bool TXInfo::saveToFile(const QString &fileName) const
{
   return bs::core::wallet::ExportTxToFile(txReq_, fileName) == bs::error::ErrorCode::NoError;
}

bool TXInfo::loadSignedTx(const QString &fileName)
{
   QFile f(fileName);
   bool result = f.open(QIODevice::ReadOnly);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "can't open file ('{}') to load signed offline request", fileName.toStdString());
      return false;
   }
   auto loadedTxs = bs::core::wallet::ParseOfflineTXFile(f.readAll().toStdString());
   if (loadedTxs.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "loading signed offline request failed from '{}'", fileName.toStdString());
      return false;
   }
   if (loadedTxs.size() != 1 || loadedTxs.front().prevStates.size() != 1) {
      SPDLOG_LOGGER_ERROR(logger_, "invalid signed offline request in '{}'", fileName.toStdString());
      return false;
   }

   // check signed tx
   if (!txReq_.isSourceOfTx(Tx(loadedTxs.front().prevStates.front()))) {
      SPDLOG_LOGGER_ERROR(logger_, "sign request not equal to signed tx in '{}'", fileName.toStdString());
      return false;
   }

   txReqSigned_ = loadedTxs.front();

   emit dataChanged();
   return true;
}

QString TXInfo::getSaveOfflineTxFileName()
{
   return QStringLiteral("%1_%2.bin")
     .arg(walletId())
     .arg(QDateTime::currentDateTime().toSecsSinceEpoch());
}

SecureBinaryData TXInfo::getSignedTx()
{
   if (txReqSigned_.prevStates.empty()) {
      SPDLOG_LOGGER_ERROR(logger_, "missing signed offline request prevStates[1]");
      return {};
   }
   return txReqSigned_.prevStates.front();
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

QStringList TXInfo::counterPartyRecipients() const
{
   // Get recipients not listed in our wallets
   // Usable for settlement tx dialog

   std::vector<std::shared_ptr<ScriptRecipient>> recipientsList;
   recipientsList = txReq_.getRecipients(containsCounterPartyAddressCb_);

   QStringList result;
   for (const auto &recip : recipientsList) {
      const auto addrStr = bs::Address::fromRecipient(recip);
      result.push_back(QString::fromStdString(addrStr.display()));
   }

   result.removeDuplicates();
   return result;
}

QStringList TXInfo::allRecipients() const
{
   // Get all recipients from this tx
   // Usable for regular tx sign dialog

   std::vector<std::shared_ptr<ScriptRecipient>> recipientsList;
   recipientsList = txReq_.getRecipients([](const bs::Address &){ return true; });

   QStringList result;
   for (const auto &recip : recipientsList) {
      const auto addrStr = bs::Address::fromRecipient(recip);
      result.push_back(QString::fromStdString(addrStr.display()));
   }

   result.removeDuplicates();
   return result;
}

QString TXInfo::counterPartyCCReceiverAddress() const
{
   // Usable to find counterparty CC address where CC coins sent when we sell
   const uint64_t amountCCSpent = txReq_.totalSpent(containsAnyOurCCAddressCb_);
   return counterPartyReceiverAddress(amountCCSpent);
}

QString TXInfo::counterPartyXBTReceiverAddress() const
{
   // Find address where xbt coins sent (for CC settlements)
   const uint64_t amountXBTSpent = txReq_.totalSpent(containsAnyOurXbtAddressCb_) - txReq_.getFee();
   return counterPartyReceiverAddress(amountXBTSpent);
}

QString TXInfo::counterPartyReceiverAddress(uint64_t amount) const
{
   for (const auto &addressStr : counterPartyRecipients()) {
      const auto address = bs::Address::fromAddressString(addressStr.toStdString());

      // Set removeDuplicatedRecipients to fix problem with duplicated recipients.
      // Looks like this is only needed for requesters.
      const bool removeDuplicatedRecipients = true;
      const uint64_t amountReceivedOn = txReq_.amountReceivedOn(address, removeDuplicatedRecipients) ;

      if (amountReceivedOn == amount) {
         return addressStr;
      }
   }
   return {};
}

QStringList TXInfo::inputsXBT() const
{
   return inputs(bs::core::wallet::Type::Bitcoin);
}

QStringList TXInfo::inputsCC() const
{
   return inputs(bs::core::wallet::Type::ColorCoin);
}
