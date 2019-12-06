/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "DealerCCSettlementContainer.h"

#include <spdlog/spdlog.h>

#include "BSErrorCodeStrings.h"
#include "CheckRecipSigner.h"
#include "SignContainer.h"
#include "SignerDefs.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <QApplication>
#include <QPointer>

using namespace bs::sync;

DealerCCSettlementContainer::DealerCCSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
   , const bs::network::Order &order
   , const std::string &quoteReqId, uint64_t lotSize
   , const bs::Address &genAddr
   , const std::string &ownRecvAddr
   , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , bs::UtxoReservationToken utxoRes)
   : bs::SettlementContainer()
   , logger_(logger)
   , order_(order)
   , lotSize_(lotSize)
   , genesisAddr_(genAddr)
   , delivery_(order.side == bs::network::Side::Sell)
   , xbtWallet_(xbtWallet)
   , signingContainer_(container)
   , walletsMgr_(walletsMgr)
   , txReqData_(BinaryData::CreateFromHex(order.reqTransaction))
   , ownRecvAddr_(bs::Address::fromAddressString(ownRecvAddr))
   , orderId_(QString::fromStdString(order.clOrderId))
   , signer_(armory)
{
   utxoRes_ = std::move(utxoRes);

   if (lotSize == 0) {
      throw std::logic_error("invalid lotSize");
   }

   ccWallet_ = walletsMgr->getCCWallet(order.product);
   if (!ccWallet_) {
      throw std::logic_error("can't find CC wallet");
   }

   connect(this, &DealerCCSettlementContainer::genAddressVerified, this
      , &DealerCCSettlementContainer::onGenAddressVerified, Qt::QueuedConnection);
}

DealerCCSettlementContainer::~DealerCCSettlementContainer() = default;

bs::sync::PasswordDialogData DealerCCSettlementContainer::toPasswordDialogData(QDateTime timestamp) const
{
   bs::sync::PasswordDialogData dialogData = SettlementContainer::toPasswordDialogData(timestamp);
   dialogData.setValue(PasswordDialogData::IsDealer, true);
   dialogData.setValue(PasswordDialogData::Market, "CC");
   dialogData.setValue(PasswordDialogData::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementDealer));
   dialogData.setValue(PasswordDialogData::LotSize, static_cast<int>(lotSize_));

   if (side() == bs::network::Side::Sell) {
      dialogData.setValue(PasswordDialogData::Title, tr("Settlement Delivery"));
   }
   else {
      dialogData.setValue(PasswordDialogData::Title, tr("Settlement Payment"));
   }

   // rfq details
   QString qtyProd = UiUtils::XbtCurrency;

   dialogData.setValue(PasswordDialogData::Price, UiUtils::displayPriceCC(price()));

   // tx details
   if (side() == bs::network::Side::Buy) {
      dialogData.setValue(PasswordDialogData::TxInputProduct, UiUtils::XbtCurrency);
   }
   else {
      dialogData.setValue(PasswordDialogData::TxInputProduct, product());
   }

   // settlement details
   dialogData.setValue(PasswordDialogData::DeliveryUTXOVerified, genAddrVerified_);
   dialogData.setValue(PasswordDialogData::SigningAllowed, genAddrVerified_);

   dialogData.setValue(PasswordDialogData::RecipientsListVisible, true);
   dialogData.setValue(PasswordDialogData::InputsListVisible, true);

   return dialogData;
}

bool DealerCCSettlementContainer::startSigning(QDateTime timestamp)
{
   if (!ccWallet_ || !xbtWallet_) {
      logger_->error("[DealerCCSettlementContainer::accept] failed to validate counterparty's TX - aborting");
      sendFailed();
      return false;
   }

   const auto &cbTx = [this, handle = validityFlag_.handle(), logger = logger_](bs::error::ErrorCode result, const BinaryData &signedTX) {
      if (!handle.isValid()) {
         SPDLOG_LOGGER_ERROR(logger, "failed to sign TX half, already destroyed");
         return;
      }

      if (result == bs::error::ErrorCode::NoError) {
         emit signTxRequest(orderId_, signedTX.toHexStr());
         emit completed();
         // FIXME: Does not work as expected as signedTX txid is different from combined txid
         //wallet_->setTransactionComment(signedTX, txComment());
      }
      else if (result == bs::error::ErrorCode::TxCanceled) {
         // FIXME
         // Not clear what's wrong here, and what should be fixed
         sendFailed();
      }
      else {
         SPDLOG_LOGGER_ERROR(logger_, "failed to sign TX half: {}", bs::error::ErrorCodeToString(result).toStdString());
         emit error(tr("TX half signing failed\n: %1").arg(bs::error::ErrorCodeToString(result)));
         sendFailed();
      }
   };

   txReq_.prevStates = { txReqData_ };
   txReq_.populateUTXOs = true;
   txReq_.inputs = bs::UtxoReservation::instance()->get(utxoRes_.reserveId());

   txReq_.walletIds.clear();
   for (const auto &input : txReq_.inputs) {
      const auto addr = bs::Address::fromUTXO(input);
      const auto wallet = walletsMgr_->getWalletByAddress(addr);
      if (!wallet) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find wallet from UTXO");
         continue;
      }
      txReq_.walletIds.push_back(wallet->walletId());
   }

   //Waiting for TX half signing...
   SPDLOG_LOGGER_DEBUG(logger_, "signing with {} inputs", txReq_.inputs.size());
   return (signingContainer_->signSettlementPartialTXRequest(txReq_, toPasswordDialogData(timestamp), cbTx) > 0);
}

void DealerCCSettlementContainer::activate()
{
   try {
      signer_.deserializeState(txReqData_);
      foundRecipAddr_ = signer_.findRecipAddress(ownRecvAddr_, [this](uint64_t value, uint64_t valReturn, uint64_t valInput) {
         if ((order_.side == bs::network::Side::Buy) && qFuzzyCompare(order_.quantity, value / lotSize_)) {
            amountValid_ = true; //valInput == (value + valReturn);
         }
         else if ((order_.side == bs::network::Side::Sell) &&
         (value == static_cast<uint64_t>(order_.quantity * order_.price * BTCNumericTypes::BalanceDivider))) {
            amountValid_ = true; //valInput > (value + valReturn);
         }
      });
   }
   catch (const std::exception &e) {
      logger_->error("Signer deser exc: {}", e.what());
      emit genAddressVerified(false);
      return;
   }

   if (!foundRecipAddr_ || !amountValid_) {
      logger_->warn("[DealerCCSettlementContainer::activate] requester's TX verification failed: {}/{}"
         , foundRecipAddr_, amountValid_);
      ccWallet_ = nullptr;
      xbtWallet_ = nullptr;
      emit genAddressVerified(false);
   }
   else if (order_.side == bs::network::Side::Buy) {
      //Waiting for genesis address verification to complete...
      const auto &cbHasInput = [this](bool has) {
         emit genAddressVerified(has);
      };
      signer_.hasInputAddress(genesisAddr_, cbHasInput, lotSize_);
   }
   else {
      emit genAddressVerified(true);
   }

   startTimer(kWaitTimeoutInSec);
}

void DealerCCSettlementContainer::onGenAddressVerified(bool addressVerified)
{
   genAddrVerified_ = addressVerified;
   if (!addressVerified) {
      logger_->warn("[DealerCCSettlementContainer::onGenAddressVerified] counterparty's TX is unverified");
      emit error(tr("Failed to verify counterparty's transaction"));
      ccWallet_ = nullptr;
      xbtWallet_ = nullptr;
   }

   bs::sync::PasswordDialogData pd;
   pd.setValue(PasswordDialogData::SettlementId, id());
   pd.setValue(PasswordDialogData::DeliveryUTXOVerified, addressVerified);
   pd.setValue(PasswordDialogData::SigningAllowed, addressVerified);

   signingContainer_->updateDialogData(pd);
}

bool DealerCCSettlementContainer::cancel()
{
   signingContainer_->CancelSignTx(id());
   cancelled_ = true;

   SettlementContainer::releaseUtxoRes();

   return true;
}

std::string DealerCCSettlementContainer::txComment()
{
   return std::string(bs::network::Side::toString(order_.side))
         + " " + order_.security + " @ " + std::to_string(order_.price);
}

void DealerCCSettlementContainer::sendFailed()
{
   SettlementContainer::releaseUtxoRes();
   emit failed();
}
