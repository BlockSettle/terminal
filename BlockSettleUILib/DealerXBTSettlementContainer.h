#ifndef __DEALER_XBT_SETTLEMENT_CONTAINER_H__
#define __DEALER_XBT_SETTLEMENT_CONTAINER_H__

#include <memory>
#include <unordered_set>
#include "AddressVerificator.h"
#include "SettlementContainer.h"
#include "SettlementWallet.h"
#include "TransactionData.h"

namespace spdlog {
   class logger;
}
class SignContainer;
class QuoteProvider;
class TransactionData;
class WalletsManager;


class DealerXBTSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   DealerXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &, const bs::network::Order &
      , const std::shared_ptr<WalletsManager> &, const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<TransactionData> &, const std::unordered_set<std::string> &bsAddresses
      , const std::shared_ptr<SignContainer> &, bool autoSign);
   ~DealerXBTSettlementContainer() override = default;

   bool accept(const std::string& password = {}) override;
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
   double price() const { return order_.price; }
   double amount() const override { return amount_; }

   bool weSell() const { return weSell_; }
   uint64_t fee() const { return fee_; }
   std::string walletName() const;
   bs::Address receiveAddress() const;

   std::shared_ptr<bs::Wallet> GetWallet() const { return transactionData_->GetWallet(); }

signals:
   void cptyAddressStateChanged(AddressVerificationState);
   void payInDetected(int confirmationsNumber, const BinaryData &txHash);

private slots:
   void onPayInDetected(int confirmationsNumber, const BinaryData &txHash);
   void onPayOutDetected(int confirmationsNumber, bs::PayoutSigner::Type signedBy);

   void onTXSigned(unsigned int id, BinaryData signedTX, std::string errMsg);

private:
   void onCptyVerified();
   void sendBuyReqPayout();

private:
   const bs::network::Order   order_;
   const bool     weSell_;
   std::string    comment_;
   const double   amount_;
   const bool     autoSign_;
   std::shared_ptr<spdlog::logger>              logger_;
   std::shared_ptr<TransactionData>             transactionData_;
   std::shared_ptr<bs::SettlementWallet>        settlWallet_;
   std::shared_ptr<bs::SettlementAddressEntry>  settlAddr_;
   std::shared_ptr<bs::SettlementMonitor>       settlMonitor_;
   std::shared_ptr<AddressVerificator>          addrVerificator_;
   std::shared_ptr<SignContainer>               signingContainer_;
   AddressVerificationState                     cptyAddressState_ = AddressVerificationState::InProgress;
   std::string settlIdStr_;
   BinaryData  authKey_, reqAuthKey_;
   bool        payInDetected_ = false;
   bool        payInSent_ = false;
   uint64_t    fee_ = 0;

   unsigned int   payinSignId_ = 0;
   unsigned int   payoutSignId_ = 0;
};

#endif // __DEALER_XBT_SETTLEMENT_CONTAINER_H__
