#ifndef __DEALER_XBT_SETTLEMENT_CONTAINER_H__
#define __DEALER_XBT_SETTLEMENT_CONTAINER_H__

#include "AddressVerificator.h"
#include "BSErrorCode.h"
#include "SettlementContainer.h"
#include "SettlementMonitor.h"
#include "TransactionData.h"

#include <memory>
#include <unordered_set>

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class SettlementWallet;
      class Wallet;
      class WalletsManager;
   }
}
class ArmoryConnection;
class SignContainer;
class QuoteProvider;
class TransactionData;


class DealerXBTSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   DealerXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &, const bs::network::Order &
      , const std::shared_ptr<bs::sync::WalletsManager> &, const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<TransactionData> &, const std::unordered_set<std::string> &bsAddresses
      , const std::shared_ptr<SignContainer> &, const std::shared_ptr<ArmoryConnection> &);
   ~DealerXBTSettlementContainer() override;

   bool cancel() override;

   bool isAcceptable() const override;

   void activate() override;
   void deactivate() override;

   std::string id() const override { return order_.settlementId; }
   bs::network::Asset::Type assetType() const override { return order_.assetType; }
   std::string security() const override { return order_.security; }
   std::string product() const override { return order_.product; }
   bs::network::Side::Type side() const override { return order_.side; }
   double quantity() const override { return order_.quantity; }
   double price() const override { return order_.price; }
   double amount() const override { return amount_; }
   bs::sync::PasswordDialogData toPasswordDialogData() const override;

   bool weSell() const { return weSell_; }
   uint64_t fee() const { return fee_; }
   std::string walletName() const;
   bs::Address receiveAddress() const;

   std::shared_ptr<bs::sync::Wallet> getWallet() const { return transactionData_->getWallet(); }

signals:
   void cptyAddressStateChanged(AddressVerificationState);
   void payInDetected(int confirmationsNumber, const BinaryData &txHash);

private slots:
   void onPayInDetected(int confirmationsNumber, const BinaryData &txHash);
   void onPayOutDetected(bs::PayoutSigner::Type signedBy);

   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode, std::string errMsg);

private:
   void onCptyVerified();
   void sendBuyReqPayout();
   bool startPayInSigning();
   bool startPayOutSigning();

private:
   const bs::network::Order   order_;
   const bool     weSell_;
   std::string    comment_;
   const double   amount_;
   std::shared_ptr<spdlog::logger>              logger_;
   std::shared_ptr<ArmoryConnection>            armory_;
   std::shared_ptr<TransactionData>             transactionData_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsMgr_;
   std::shared_ptr<bs::SettlementMonitorCb>     settlMonitor_;
   std::shared_ptr<AddressVerificator>          addrVerificator_;
   std::shared_ptr<SignContainer>               signingContainer_;
   AddressVerificationState                     cptyAddressState_ = AddressVerificationState::InProgress;
   bs::Address settlAddr_;
   BinaryData  settlementId_;
   BinaryData  authKey_, reqAuthKey_;
   bool        payInDetected_ = false;
   bool        payInSent_ = false;
   uint64_t    fee_ = 0;

   unsigned int   payinSignId_ = 0;
   unsigned int   payoutSignId_ = 0;
};

#endif // __DEALER_XBT_SETTLEMENT_CONTAINER_H__
