#ifndef __REQ_CC_SETTLEMENT_CONTAINER_H__
#define __REQ_CC_SETTLEMENT_CONTAINER_H__

#include <memory>
#include "CheckRecipSigner.h"
#include "MetaData.h"
#include "SettlementContainer.h"

namespace spdlog {
   class logger;
}
namespace bs {
   class UtxoReservation;
}
class ArmoryConnection;
class AssetManager;
class SignContainer;
class TransactionData;
class WalletsManager;


class ReqCCSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   ReqCCSettlementContainer(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<WalletsManager> &
      , const bs::network::RFQ &, const bs::network::Quote &
      , const std::shared_ptr<TransactionData> &);
   ~ReqCCSettlementContainer() override;

   bool accept(const SecureBinaryData &password = {}) override;
   bool cancel() override;

   bool isAcceptable() const override;

   void activate() override;
   void deactivate() override;

   std::string id() const override { return rfq_.requestId; }
   bs::network::Asset::Type assetType() const override { return rfq_.assetType; }
   std::string security() const override { return rfq_.security; }
   std::string product() const override { return rfq_.product; }
   bs::network::Side::Type side() const override { return rfq_.side; }
   double quantity() const override { return quote_.quantity; }
   double price() const override { return quote_.price; }
   double amount() const override { return quantity() * price(); }

   std::string walletName() const { return walletName_; }
   std::string walletId() const { return walletId_; }

   std::vector<bs::wallet::EncryptionType> encTypes() const { return encTypes_; }
   std::vector<SecureBinaryData> encKeys() const { return encKeys_; }
   bs::wallet::KeyRank keyRank() const { return keyRank_; }

   std::string txData() const;
   std::string txSignedData() const { return ccTxSigned_; }

signals:
   void settlementCancelled();
   void settlementAccepted();
   void sendOrder();
   void genAddrVerified(bool result, QString error);
   void paymentVerified(bool result, QString error);
   void walletInfoReceived();

private slots:
   void onHDWalletInfo(unsigned int id, std::vector<bs::wallet::EncryptionType>
      , std::vector<SecureBinaryData> encKeys, bs::wallet::KeyRank);
   void onTXSigned(unsigned int id, BinaryData signedTX, std::string error, bool cancelledByUser);

private:
   bool createCCUnsignedTXdata();
   bool createCCSignedTXdata(const SecureBinaryData &password);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<TransactionData>    transactionData_;
   std::shared_ptr<AssetManager>       assetMgr_;
   std::shared_ptr<WalletsManager>     walletsMgr_;
   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;
   const bs::Address          genAddress_;
   const std::string          dealerAddress_;
   bs::CheckRecipSigner       signer_;

   std::string    walletName_;
   std::string    walletId_;
   uint64_t       lotSize_;
   unsigned int   ccSignId_ = 0;
   unsigned int   infoReqId_ = 0;
   bool           userKeyOk_ = false;

   BinaryData                 dealerTx_;
   BinaryData                 requesterTx_;
   bs::wallet::TXSignRequest  ccTxData_;
   std::string                ccTxSigned_;

   std::shared_ptr<bs::UtxoReservation::Adapter>   utxoAdapter_;

   std::vector<bs::wallet::EncryptionType>   encTypes_;
   std::vector<SecureBinaryData>             encKeys_;
   bs::wallet::KeyRank                       keyRank_;
};

#endif // __REQ_CC_SETTLEMENT_CONTAINER_H__
