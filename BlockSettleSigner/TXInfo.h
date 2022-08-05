/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TX_INFO_H__
#define __TX_INFO_H__

#include "CoreWallet.h"
#include "Wallets/ProtobufHeadlessUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

#include "bs_signer.pb.h"

#include <memory>
#include <QObject>
#include <QStringList>

namespace bs {
namespace wallet {

// wrapper on bs::wallet::TXSignRequest
class TXInfo : public QObject
{
   Q_OBJECT

   Q_PROPERTY(bool isValid READ isValid NOTIFY dataChanged)
   Q_PROPERTY(int nbInputs READ nbInputs NOTIFY dataChanged)

   Q_PROPERTY(QStringList inputsXBT READ inputsXBT NOTIFY dataChanged)
   Q_PROPERTY(QStringList inputsCC READ inputsCC NOTIFY dataChanged)
   Q_PROPERTY(QStringList allRecipients READ allRecipients NOTIFY dataChanged)
   Q_PROPERTY(QStringList counterPartyRecipients READ counterPartyRecipients NOTIFY dataChanged)

   Q_PROPERTY(QString counterPartyCCReceiverAddress READ counterPartyCCReceiverAddress NOTIFY dataChanged)
   Q_PROPERTY(QString counterPartyXBTReceiverAddress READ counterPartyXBTReceiverAddress NOTIFY dataChanged)

   Q_PROPERTY(int txVirtSize READ txVirtSize NOTIFY dataChanged)
   Q_PROPERTY(double amount READ amount NOTIFY dataChanged)
   Q_PROPERTY(double total READ total NOTIFY dataChanged)
   Q_PROPERTY(double fee READ fee NOTIFY dataChanged)
   Q_PROPERTY(double changeAmount READ changeAmount NOTIFY dataChanged)
   Q_PROPERTY(double inputAmount READ inputAmount NOTIFY dataChanged)
   Q_PROPERTY(bool hasChange READ hasChange NOTIFY dataChanged)
   Q_PROPERTY(QString txId READ txId WRITE setTxId NOTIFY dataChanged)
   Q_PROPERTY(QString walletId READ walletId NOTIFY dataChanged)

   Q_PROPERTY(bool isOfflineTxSigned READ isOfflineTxSigned NOTIFY dataChanged)
   Q_PROPERTY(double inputAmountFull READ inputAmountFull NOTIFY dataChanged)
   Q_PROPERTY(double outputAmountFull READ outputAmountFull NOTIFY dataChanged)

public:
   TXInfo() : QObject(), txReq_() {}
   TXInfo(const bs::core::wallet::TXSignRequest &txReq, const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<spdlog::logger> &logger);
   TXInfo(const Blocksettle::Communication::headless::SignTxRequest &txRequest, const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<spdlog::logger> &logger);
   TXInfo(const TXInfo &src);

   bool isValid() const { return txReq_.isValid(); }
   size_t nbInputs() const { return inputsXBT().size(); }

   QStringList inputsXBT() const;
   QStringList inputsCC() const;
   QStringList counterPartyRecipients() const;
   QStringList allRecipients() const;

   QString counterPartyCCReceiverAddress() const;
   QString counterPartyXBTReceiverAddress() const;
   QString counterPartyReceiverAddress(uint64_t amount) const;

   size_t txVirtSize() const { return txReq_.estimateTxVirtSize(); }
   double amount() const { return txReq_.amount(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   double total() const { return txReq_.totalSpent(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   double fee() const { return txReq_.getFee() / BTCNumericTypes::BalanceDivider; }
   bool hasChange() const { return (changeAmount() > 0); }
   double changeAmount() const { return txReq_.changeAmount(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   double inputAmount() const { return txReq_.inputAmount(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   QString txId() const { return txId_; }
   void setTxId(const QString &);
   QString walletId() const;
   bool isOfflineTxSigned() { return txReqSigned_.isValid(); }
   double inputAmountFull() const;
   double outputAmountFull() const;

   Q_INVOKABLE double amountXBTReceived() const;

   Q_INVOKABLE bool saveToFile(const QString &fileName) const;
   Q_INVOKABLE bool loadSignedTx(const QString &fileName);

   Q_INVOKABLE QString getSaveOfflineTxFileName();
   Q_INVOKABLE SecureBinaryData getSignedTx();

signals:
   void dataChanged();

private:
   void init();
   QStringList inputs(core::wallet::Type leafType) const;

private:
   const bs::core::wallet::TXSignRequest  txReq_;
   bs::core::wallet::TXSignRequest  txReqSigned_;

   QString  txId_;

   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_ = nullptr; // nullptr init required for default constructor
   std::shared_ptr<spdlog::logger> logger_ = nullptr;

   using ContainsAddressCb = const std::function<bool(const bs::Address &)>;
   ContainsAddressCb containsThisAddressCb_ = [this](const bs::Address &address){
      for (const auto &walletId : txReq_.walletIds) {
         const auto &wallet = walletsMgr_->getWalletById(walletId);
         if (wallet) {
            if (wallet->containsAddress(address)) {
               return true;
            }
         } else {
            // Payout sets HW wallet ID
            const auto &hdWallet = walletsMgr_->getHDWalletById(walletId);
            assert(hdWallet);
            for (auto leaf : hdWallet->getLeaves()) {
               if (leaf->containsAddress(address)) {
                  return true;
               }
            }
         }
      }

      return false;
   };

   ContainsAddressCb containsAnyOurXbtAddressCb_ = [this](const bs::Address &address){
      return containsAddressImpl(address, core::wallet::Type::Bitcoin);
   };

   ContainsAddressCb containsAnyOurCCAddressCb_ = [this](const bs::Address &address){
      return containsAddressImpl(address, core::wallet::Type::ColorCoin);
   };

   ContainsAddressCb containsCounterPartyAddressCb_ = [this](const bs::Address &address){
      return notContainsAddressImpl(address);
   };

   bool containsAddressImpl(const bs::Address &address, core::wallet::Type walletType) const;
   bool notContainsAddressImpl(const bs::Address &address) const;
};

}  //namespace wallet
}  //namespace bs


#endif // __TX_INFO_H__
