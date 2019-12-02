/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __DEALER_CC_SETTLEMENT_CONTAINER_H__
#define __DEALER_CC_SETTLEMENT_CONTAINER_H__

#include <memory>
#include "AddressVerificator.h"
#include "CheckRecipSigner.h"
#include "SettlementContainer.h"
#include "UtxoReservationToken.h"
#include "CoreWallet.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
}
class ArmoryConnection;
class SignContainer;


class DealerCCSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   DealerCCSettlementContainer(const std::shared_ptr<spdlog::logger> &, const bs::network::Order &
      , const std::string &quoteReqId, uint64_t lotSize
      , const bs::Address &genAddr
      , const std::string &ownRecvAddr
      , const std::shared_ptr<bs::sync::hd::Wallet> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , bs::UtxoReservationToken utxoRes);
   ~DealerCCSettlementContainer() override;

   bool startSigning();
   bool cancel() override;

   void activate() override;
   void deactivate() override { stopTimer(); }

   std::string id() const override { return orderId_.toStdString(); }
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

   bool isDelivery() const { return delivery_; }

signals:
   void signTxRequest(const QString &orderId, std::string txData);
   void genAddressVerified(bool result);

private slots:
   void onGenAddressVerified(bool result);

private:
   std::string txComment();
   void sendFailed();

private:
   std::shared_ptr<spdlog::logger>     logger_;
   const bs::network::Order   order_;
   const uint64_t             lotSize_;
   const bs::Address          genesisAddr_;
   const bool                 delivery_;
   std::shared_ptr<bs::sync::hd::Wallet>   xbtWallet_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   const BinaryData  txReqData_;
   const bs::Address ownRecvAddr_;
   const QString     orderId_;
   bool              foundRecipAddr_ = false;
   bool              amountValid_ = false;
   bool              genAddrVerified_ = true;
   unsigned int      signId_ = 0;
   bool              cancelled_ = false;
   bs::CheckRecipSigner signer_;
   bs::core::wallet::TXSignRequest txReq_;
   bool isGetAddressValid_ = false;
   std::shared_ptr<bs::sync::Wallet>         ccWallet_;

};

#endif // __DEALER_CC_SETTLEMENT_CONTAINER_H__
