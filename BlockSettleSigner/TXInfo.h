#ifndef __TX_INFO_H__
#define __TX_INFO_H__

#include "CoreWallet.h"
#include "ProtobufHeadlessUtils.h"
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
   Q_PROPERTY(QStringList recipients READ recipients NOTIFY dataChanged)

   Q_PROPERTY(int txVirtSize READ txVirtSize NOTIFY dataChanged)
   Q_PROPERTY(double amount READ amount NOTIFY dataChanged)
   Q_PROPERTY(double total READ total NOTIFY dataChanged)
   Q_PROPERTY(double fee READ fee NOTIFY dataChanged)
   Q_PROPERTY(double changeAmount READ changeAmount NOTIFY dataChanged)
   Q_PROPERTY(double inputAmount READ inputAmount NOTIFY dataChanged)
   Q_PROPERTY(bool hasChange READ hasChange NOTIFY dataChanged)
   Q_PROPERTY(QString txId READ txId WRITE setTxId NOTIFY dataChanged)
   Q_PROPERTY(QString walletId READ walletId NOTIFY dataChanged)

public:
   TXInfo() : QObject(), txReq_() {}
   TXInfo(const bs::core::wallet::TXSignRequest &txReq, const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<spdlog::logger> &logger);
   TXInfo(const Blocksettle::Communication::headless::SignTxRequest &txRequest, const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<spdlog::logger> &logger);
   TXInfo(const TXInfo &src);

   bool isValid() const { return txReq_.isValid(); }
   size_t nbInputs() const { return txReq_.inputs.size(); }

   QStringList inputsXBT() const;
   QStringList inputsCC() const;
   QStringList recipients() const;

   size_t txVirtSize() const { return txReq_.estimateTxVirtSize(); }
   double amount() const { return txReq_.amount(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   double total() const { return txReq_.totalSpent(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   double fee() const { return txReq_.fee / BTCNumericTypes::BalanceDivider; }
   bool hasChange() const { return (changeAmount() > 0); }
   double changeAmount() const { return txReq_.changeAmount(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   double inputAmount() const { return txReq_.inputAmount(containsThisAddressCb_) / BTCNumericTypes::BalanceDivider; }
   QString txId() const { return txId_; }
   void setTxId(const QString &);
   QString walletId() const { return QString::fromStdString(txReq_.walletIds.front()); }

   Q_INVOKABLE double amountCCReceived(const QString &cc) const;
   Q_INVOKABLE double amountCCSent() const { return amount(); }

   Q_INVOKABLE double amountXBTReceived() const;

signals:
   void dataChanged();

private:
   void init();

   QStringList inputs(core::wallet::Type coinType) const;

private:
   const bs::core::wallet::TXSignRequest  txReq_;
   QString  txId_;

   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_ = nullptr; // nullptr init required for default constructor
   std::shared_ptr<spdlog::logger>  logger_ = nullptr;

   const std::function<bool(const bs::Address &)> containsThisAddressCb_ = [this](const bs::Address &address){
      if (txReq_.walletIds.empty()) {
         return false;
      }

      const auto &hdWallet = walletsMgr_->getHDWalletById(txReq_.walletIds.front());
      if (hdWallet) {
         for (auto leaf : hdWallet->getLeaves()) {
            if (leaf->containsAddress(address)) {
               return true;
            }
         }
      }
      else {
         const auto &wallet = walletsMgr_->getWalletById(txReq_.walletIds.front());
         if (wallet) {
            return wallet->containsAddress(address);
         }
      }
      return false;
   };

   const std::function<bool(const bs::Address &)> containsAnyOurXbtAddressCb_ = [this](const bs::Address &address){
      return containsAddressImpl(address, core::wallet::Type::Bitcoin);
   };

   const std::function<bool(const bs::Address &)> containsAnyOurCCAddressCb_ = [this](const bs::Address &address){
      return containsAddressImpl(address, core::wallet::Type::ColorCoin);
   };

   const std::function<bool(const bs::Address &)> containsCounterPartyAddressCb_ = [this](const bs::Address &address){
      return notContainsAddressImpl(address);
   };

   bool containsAddressImpl(const bs::Address &address, core::wallet::Type coinType) const;
   bool notContainsAddressImpl(const bs::Address &address) const;
};

}  //namespace wallet
}  //namespace bs


#endif // __TX_INFO_H__
