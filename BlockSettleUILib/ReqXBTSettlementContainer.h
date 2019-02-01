#ifndef __REQ_XBT_SETTLEMENT_CONTAINER_H__
#define __REQ_XBT_SETTLEMENT_CONTAINER_H__

#include <memory>
#include <unordered_set>
#include "AddressVerificator.h"
#include "AuthAddress.h"
#include "MetaData.h"
#include "SettlementContainer.h"
#include "SettlementMonitor.h"
#include "SettlementWallet.h"
#include "UtxoReservation.h"
#include "TransactionData.h"

namespace spdlog {
   class logger;
}
class AddressVerificator;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class SignContainer;
class QuoteProvider;
class TransactionData;
class WalletsManager;


class ReqXBTSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   ReqXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<AuthAddressManager> &, const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<SignContainer> &, const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<WalletsManager> &, const bs::network::RFQ &
      , const bs::network::Quote &, const std::shared_ptr<TransactionData> &);
   ~ReqXBTSettlementContainer() override;

   void OrderReceived();

   bool accept(const SecureBinaryData &password = {}) override;
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
   std::string authWalletName() const { return authWalletName_; }
   std::string authWalletId() const { return authWalletId_; }
   std::string walletId() const { return walletId_; }
   uint64_t fee() const { return fee_; }
   bool weSell() const { return clientSells_; }
   bool isSellFromPrimary() const { return sellFromPrimary_; }
   bool userKeyOk() const { return userKeyOk_; }
   bool payinReceived() const { return !payinData_.isNull(); }

   std::vector<bs::wallet::EncryptionType> encTypes() const { return encTypes_; }
   std::vector<bs::wallet::EncryptionType> authEncTypes() const { return encTypesAuth_; }
   std::vector<SecureBinaryData> encKeys() const { return encKeys_; }
   std::vector<SecureBinaryData> authEncKeys() const { return encKeysAuth_; }
   bs::wallet::KeyRank keyRank() const { return keyRank_; }
   bs::wallet::KeyRank authKeyRank() const { return keyRankAuth_; }

signals:
   void settlementCancelled();
   void settlementAccepted();
   void acceptQuote(std::string reqId, std::string hexPayoutTx);
   void DealerVerificationStateChanged(AddressVerificationState);
   void retry();
   void stop();
   void authWalletInfoReceived();

private slots:
   void onHDWalletInfo(unsigned int id, std::vector<bs::wallet::EncryptionType>
      , std::vector<SecureBinaryData> encKeys, bs::wallet::KeyRank);
   void onTXSigned(unsigned int id, BinaryData signedTX, std::string error, bool cancelledByUser);
   void onTimerExpired();
   void onPayInZCDetected();
   void onPayoutZCDetected(int confNum, bs::PayoutSigner::Type);

protected:
   void zcReceived(const std::vector<bs::TXEntry>) override;

private:
   unsigned int createPayoutTx(const BinaryData& payinHash, double qty, const bs::Address &recvAddr
      , const SecureBinaryData &password);
   void payoutOnCancel();
   void detectDealerTxs();
   void acceptSpotXBT(const SecureBinaryData &password);
   void dealerVerifStateChanged(AddressVerificationState);

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<AuthAddressManager>    authAddrMgr_;
   std::shared_ptr<AssetManager>          assetMgr_;
   std::shared_ptr<WalletsManager>        walletsMgr_;
   std::shared_ptr<SignContainer>         signContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<TransactionData>       transactionData_;
   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;

   std::shared_ptr<bs::SettlementAddressEntry>     settlAddr_;
   std::shared_ptr<AddressVerificator>       addrVerificator_;
   std::shared_ptr<bs::SettlementMonitorCb>        monitor_;
   std::shared_ptr<bs::UtxoReservation::Adapter>   utxoAdapter_;

   AddressVerificationState   dealerVerifState_ = AddressVerificationState::InProgress;

   std::vector<bs::wallet::EncryptionType>   encTypes_, encTypesAuth_;
   std::vector<SecureBinaryData>             encKeys_, encKeysAuth_;
   bs::wallet::KeyRank                       keyRank_, keyRankAuth_;

   double            amount_;
   std::string       fxProd_;
   std::string       authWalletName_;
   std::string       authWalletId_;
   std::string       walletId_;
   uint64_t          fee_;
   SecureBinaryData  payoutPassword_;
   BinaryData        settlementId_;
   BinaryData        userKey_;
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
};

#endif // __REQ_XBT_SETTLEMENT_CONTAINER_H__
