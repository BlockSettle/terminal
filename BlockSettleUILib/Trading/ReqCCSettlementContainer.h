#ifndef __REQ_CC_SETTLEMENT_CONTAINER_H__
#define __REQ_CC_SETTLEMENT_CONTAINER_H__

#include <memory>
#include "CheckRecipSigner.h"
#include "SettlementContainer.h"
#include "CommonTypes.h"
#include "CoreWallet.h"
#include "QWalletInfo.h"
#include "UtxoReservationToken.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class ArmoryConnection;
class AssetManager;
class SignContainer;


class ReqCCSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   ReqCCSettlementContainer(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const bs::network::RFQ &
      , const bs::network::Quote &
      , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
      , const std::map<UTXO, std::string> &manualXbtInputs
      , bs::UtxoReservationToken utxoRes);
   ~ReqCCSettlementContainer() override;

   bool cancel() override;

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
   bs::sync::PasswordDialogData toPasswordDialogData() const override;

   bs::hd::WalletInfo walletInfo() const { return walletInfo_; }
   std::string txData() const;
   std::string txSignedData() const { return ccTxSigned_; }

   bool startSigning();

signals:
   void settlementCancelled();

   void sendOrder();

   void txSigned();
   void genAddressVerified(bool result, QString error);
   void paymentVerified(bool result, QString error);
   void walletInfoReceived();

private slots:
   void onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo& walletInfo);
   void onGenAddressVerified(bool addressVerified, const QString &error);

private:
   // read comments in source code
   bool createCCUnsignedTXdata();
   std::string txComment();

   void AcceptQuote();

private:
   std::shared_ptr<spdlog::logger>           logger_;
   std::shared_ptr<SignContainer>            signingContainer_;
   std::shared_ptr<bs::sync::hd::Wallet>     xbtWallet_;
   std::vector<std::shared_ptr<bs::sync::Wallet>> xbtLeaves_;
   std::shared_ptr<bs::sync::Wallet>         ccWallet_;
   std::shared_ptr<AssetManager>             assetMgr_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;
   const bs::Address          genAddress_;
   const std::string          dealerAddress_;
   bs::CheckRecipSigner       signer_;

   const uint64_t lotSize_;
   unsigned int   ccSignId_ = 0;
   unsigned int   infoReqId_ = 0;
   bool           userKeyOk_ = false;

   BinaryData                 dealerTx_;
   bs::core::wallet::TXSignRequest  ccTxData_;
   std::string                ccTxSigned_;
   bool                       genAddrVerified_ = false;

   bs::hd::WalletInfo walletInfo_;
   std::map<UTXO, std::string> manualXbtInputs_;

   bs::UtxoReservationToken utxoRes_;

};

#endif // __REQ_CC_SETTLEMENT_CONTAINER_H__
