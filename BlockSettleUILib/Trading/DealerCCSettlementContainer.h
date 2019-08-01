#ifndef __DEALER_CC_SETTLEMENT_CONTAINER_H__
#define __DEALER_CC_SETTLEMENT_CONTAINER_H__

#include <memory>
#include "AddressVerificator.h"
#include "CheckRecipSigner.h"
#include "SettlementContainer.h"
#include "UtxoReservation.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class Wallet;
   }
}
class ArmoryConnection;
class SignContainer;
class TransactionData;


class DealerCCSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   DealerCCSettlementContainer(const std::shared_ptr<spdlog::logger> &, const bs::network::Order &
      , const std::string &quoteReqId, uint64_t lotSize, const bs::Address &genAddr, const std::string &ownRecvAddr
      , const std::shared_ptr<TransactionData> &, const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &, bool autoSign);
   ~DealerCCSettlementContainer() override;

   bool startSigning();
   bool cancel() override;

   bool isAcceptable() const override;

   void activate() override;
   void deactivate() override { stopTimer(); }

   std::string id() const override { return quoteReqId_; }
   bs::network::Asset::Type assetType() const override { return order_.assetType; }
   std::string security() const override { return order_.security; }
   std::string product() const override { return order_.product; }
   bs::network::Side::Type side() const override { return order_.side; }
   double quantity() const override { return order_.quantity; }
   double price() const override { return order_.price; }
   double amount() const override { return quantity(); }
   bs::sync::PasswordDialogData toPasswordDialogData() const override;

   bool foundRecipAddr() const { return foundRecipAddr_; }
   bool isAmountValid() const { return amountValid_; }

   QString GetSigningWalletName() const;

   std::shared_ptr<bs::sync::Wallet> GetSigningWallet() const { return wallet_; }

   bool isDelivery() const { return delivery_; }

signals:
   void signTxRequest(const QString &orderId, std::string txData);
   void genAddressVerified(bool result);

private slots:
   void onGenAddressVerified(bool result);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   const bs::network::Order   order_;
   const std::string          quoteReqId_;
   const uint64_t             lotSize_;
   const bs::Address          genesisAddr_;
   const bool                 autoSign_;
   const bool                 delivery_;
   std::shared_ptr<TransactionData>    transactionData_;
   std::shared_ptr<bs::sync::Wallet>   wallet_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<bs::UtxoReservation::Adapter>   utxoAdapter_;
   const BinaryData  txReqData_;
   const bs::Address ownRecvAddr_;
   const QString     orderId_;
   bool              foundRecipAddr_ = false;
   bool              amountValid_ = false;
   bool              genAddrVerified_ = true;
   unsigned int      signId_ = 0;
   bool              cancelled_ = false;
   bs::CheckRecipSigner signer_;

   QString walletName_;
};

#endif // __DEALER_CC_SETTLEMENT_CONTAINER_H__
