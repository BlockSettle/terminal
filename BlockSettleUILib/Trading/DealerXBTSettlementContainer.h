/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __DEALER_XBT_SETTLEMENT_CONTAINER_H__
#define __DEALER_XBT_SETTLEMENT_CONTAINER_H__

#include "AddressVerificator.h"
#include "BSErrorCode.h"
#include "SettlementContainer.h"

#include <memory>
#include <unordered_set>

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class SettlementWallet;
      class Wallet;
      class WalletsManager;
   }
   namespace tradeutils {
      struct Args;
   }
}
class ArmoryConnection;
class AuthAddressManager;
class QuoteProvider;
class SignContainer;


class DealerXBTSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   DealerXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &
      , const bs::network::Order &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
      , const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<AuthAddressManager> &authAddrMgr
      , const bs::Address &authAddr
      , const std::vector<UTXO> &utxosPayinFixed
      , const bs::Address &recvAddr);
   ~DealerXBTSettlementContainer() override;

   bool cancel() override;

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
   bs::sync::PasswordDialogData toPasswordDialogData(QDateTime timestamp) const override;

public slots:
   void onUnsignedPayinRequested(const std::string& settlementId);
   void onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash, QDateTime timestamp);
   void onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin, QDateTime timestamp);

signals:
   void sendUnsignedPayinToPB(const std::string& settlementId, const bs::network::UnsignedPayinData& unsignedPayinData);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

private slots:
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode, std::string errMsg);

private:
   bool startPayInSigning();

   void failWithErrorText(const QString& error);

   void initTradesArgs(bs::tradeutils::Args &args, const std::string &settlementId);

   const bs::network::Order   order_;
   std::string    fxProd_;
   const bool     weSellXbt_;
   std::string    comment_;
   const double   amount_;

   std::shared_ptr<spdlog::logger>              logger_;
   std::shared_ptr<ArmoryConnection>            armory_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsMgr_;
   std::shared_ptr<bs::sync::hd::Wallet>        xbtWallet_;
   std::shared_ptr<AddressVerificator>          addrVerificator_;
   std::shared_ptr<SignContainer>               signContainer_;
   std::shared_ptr<AuthAddressManager>          authAddrMgr_;

   AddressVerificationState                     requestorAddressState_ = AddressVerificationState::VerificationFailed;
   bs::Address settlAddr_;

   std::string settlementIdHex_;
   BinaryData  settlementId_;
   BinaryData  authKey_;
   BinaryData  reqAuthKey_;

   bs::core::wallet::TXSignRequest        unsignedPayinRequest_;

   unsigned int   payinSignId_ = 0;
   unsigned int   payoutSignId_ = 0;

   BinaryData		usedPayinHash_;

   std::vector<UTXO> utxosPayinFixed_;
   bs::Address       recvAddr_;
   bs::Address       authAddr_;
};

#endif // __DEALER_XBT_SETTLEMENT_CONTAINER_H__
