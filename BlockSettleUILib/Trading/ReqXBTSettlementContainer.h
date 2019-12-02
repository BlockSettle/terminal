/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __REQ_XBT_SETTLEMENT_CONTAINER_H__
#define __REQ_XBT_SETTLEMENT_CONTAINER_H__

#include <memory>
#include <unordered_set>
#include "AddressVerificator.h"
#include "BSErrorCode.h"
#include "QWalletInfo.h"
#include "SettlementContainer.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
   namespace tradeutils {
      struct Args;
   }
}
class AddressVerificator;
class ArmoryConnection;
class AuthAddressManager;
class SignContainer;
class QuoteProvider;


class ReqXBTSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   ReqXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const bs::network::RFQ &
      , const bs::network::Quote &
      , const bs::Address &authAddr
      , const std::map<UTXO, std::string> &utxosPayinFixed
      , const bs::Address &recvAddr);
   ~ReqXBTSettlementContainer() override;

   bool cancel() override;

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
   bs::sync::PasswordDialogData toPasswordDialogData() const override;

   void onUnsignedPayinRequested(const std::string& settlementId);
   void onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash);
   void onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin);

signals:
   void settlementCancelled();
   void settlementAccepted();
   void acceptQuote(std::string reqId, std::string hexPayoutTx);

   void sendUnsignedPayinToPB(const std::string& settlementId, const bs::network::UnsignedPayinData& unsignedPayinData);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

private slots:
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode, std::string error);
   void onTimerExpired();

private:
   void acceptSpotXBT();
   void dealerVerifStateChanged(AddressVerificationState);

   void cancelWithError(const QString& errorMessage);

   void initTradesArgs(bs::tradeutils::Args &args, const std::string &settlementId);

   std::shared_ptr<spdlog::logger>           logger_;
   std::shared_ptr<AuthAddressManager>       authAddrMgr_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<SignContainer>            signContainer_;
   std::shared_ptr<ArmoryConnection>         armory_;
   std::shared_ptr<bs::sync::hd::Wallet>     xbtWallet_;

   bs::network::RFQ           rfq_;
   bs::network::Quote         quote_;
   bs::Address                settlAddr_;

   std::shared_ptr<AddressVerificator>             addrVerificator_;

   double            amount_{};
   std::string       fxProd_;
   BinaryData        settlementId_;
   std::string       settlementIdHex_;
   BinaryData        userKey_;
   BinaryData        dealerAuthKey_;
   bs::Address       recvAddr_;
   AddressVerificationState dealerVerifState_ = AddressVerificationState::VerificationFailed;

   std::string       comment_;
   const bool        weSellXbt_;
   bool              userKeyOk_ = false;

   unsigned int      payinSignId_ = 0;
   unsigned int      payoutSignId_ = 0;

   const bs::Address authAddr_;
   bs::Address       dealerAuthAddress_;

   bs::core::wallet::TXSignRequest        unsignedPayinRequest_;
   BinaryData                    usedPayinHash_;
   std::map<UTXO, std::string>   utxosPayinFixed_;
};

#endif // __REQ_XBT_SETTLEMENT_CONTAINER_H__
