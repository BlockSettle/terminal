#ifndef __REQ_XBT_SETTLEMENT_CONTAINER_H__
#define __REQ_XBT_SETTLEMENT_CONTAINER_H__

#include <memory>
#include <unordered_set>
#include "AddressVerificator.h"
#include "BSErrorCode.h"
#include "SettlementContainer.h"
#include "SettlementMonitor.h"
#include "UtxoReservation.h"
#include "TransactionData.h"
#include "QWalletInfo.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AddressVerificator;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class SignContainer;
class QuoteProvider;
class TransactionData;


class ReqXBTSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   ReqXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const bs::network::RFQ &
      , const bs::network::Quote &
      , const std::shared_ptr<TransactionData> &
      , const bs::Address &authAddr
   );
   ~ReqXBTSettlementContainer() override;

   void OrderReceived();

   bool startSigning();
   bool cancel() override;

   bool isAcceptable() const override;

   void activate() override;
   void deactivate() override;

   std::string id() const override { return quote_.requestId; }
   bs::network::Asset::Type assetType() const override { return rfq_.assetType; }
   std::string security() const override { return rfq_.security; }
   std::string product() const override { return rfq_.product; }
   bs::network::Side::Type side() const override { return rfq_.side; }
   double quantity() const override { return quote_.quantity; }
   double price() const override { return quote_.price; }
   double amount() const override { return amount_; }

   std::string fxProduct() const { return fxProd_; }
   uint64_t fee() const { return fee_; }
   bool weSell() const { return clientSells_; }
   bool isSellFromPrimary() const { return sellFromPrimary_; }
   bool userKeyOk() const { return userKeyOk_; }
   bool payinReceived() const { return !payinData_.isNull(); }

   bs::hd::WalletInfo walletInfo() const { return walletInfo_; }
   bs::hd::WalletInfo walletInfoAuth() const { return walletInfoAuth_; }

signals:
   void settlementCancelled();
   void settlementAccepted();
   void acceptQuote(std::string reqId, std::string hexPayoutTx);
   void DealerVerificationStateChanged(AddressVerificationState);
   void retry();
   void stop();
   void authWalletInfoReceived();

private slots:
   void onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo &walletInfo);
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode, std::string error);
   void onTimerExpired();
   void onPayInZCDetected();
   void onPayoutZCDetected(int confNum, bs::PayoutSigner::Type);

private:
   unsigned int createPayoutTx(const BinaryData& payinHash, double qty, const bs::Address &recvAddr);
   void payoutOnCancel();
   void detectDealerTxs();
   void acceptSpotXBT();
   void dealerVerifStateChanged(AddressVerificationState);
   void activateProceed();

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<AuthAddressManager>    authAddrMgr_;
   std::shared_ptr<AssetManager>          assetMgr_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<SignContainer>         signContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<TransactionData>       transactionData_;
   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;
   bs::Address                settlAddr_;

   std::shared_ptr<AddressVerificator>       addrVerificator_;
   std::shared_ptr<bs::SettlementMonitorCb>        monitor_;
   std::shared_ptr<bs::UtxoReservation::Adapter>   utxoAdapter_;

   AddressVerificationState   dealerVerifState_ = AddressVerificationState::InProgress;

   bs::hd::WalletInfo walletInfo_, walletInfoAuth_;


   double            amount_;
   std::string       fxProd_;
   uint64_t          fee_;
   BinaryData        settlementId_;
   BinaryData        userKey_;
   BinaryData        dealerAuthKey_;
   bs::Address       recvAddr_;
   BinaryData        dealerTx_;
   BinaryData        requesterTx_;
   BinaryData        payinData_;
   BinaryData        payoutData_;
   std::string       dealerAddress_;
   std::string       comment_;
   const bool        clientSells_;
   bool              userKeyOk_ = false;
   bool              sellFromPrimary_ = false;
   std::atomic_bool  waitForPayout_;
   std::atomic_bool  waitForPayin_;
   unsigned int      payinSignId_ = 0;
   unsigned int      payoutSignId_ = 0;
   unsigned int      infoReqId_ = 0;
   unsigned int      infoReqIdAuth_ = 0;

   const bs::Address authAddr_;
};

#endif // __REQ_XBT_SETTLEMENT_CONTAINER_H__
