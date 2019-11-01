#include "DealerCCSettlementContainer.h"

#include <spdlog/spdlog.h>

#include "BSErrorCodeStrings.h"
#include "CheckRecipSigner.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"
#include "SignerDefs.h"

#include <QApplication>
#include <QPointer>

using namespace bs::sync;

DealerCCSettlementContainer::DealerCCSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
      , const bs::network::Order &order, const std::string &quoteReqId, uint64_t lotSize
      , const bs::Address &genAddr, const std::string &ownRecvAddr
      , const std::shared_ptr<bs::sync::Wallet> &wallet, const std::shared_ptr<SignContainer> &container
      , const std::shared_ptr<ArmoryConnection> &armory)
   : bs::SettlementContainer()
   , logger_(logger)
   , order_(order)
   , quoteReqId_(quoteReqId)
   , lotSize_(lotSize)
   , genesisAddr_(genAddr)
   , delivery_(order.side == bs::network::Side::Sell)
   , wallet_(wallet)
   , signingContainer_(container)
   , txReqData_(BinaryData::CreateFromHex(order.reqTransaction))
   , ownRecvAddr_(bs::Address::fromAddressString(ownRecvAddr))
   , orderId_(QString::fromStdString(order.clOrderId))
   , signer_(armory)
{
   connect(this, &DealerCCSettlementContainer::genAddressVerified, this
      , &DealerCCSettlementContainer::onGenAddressVerified, Qt::QueuedConnection);

   utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
   bs::UtxoReservation::addAdapter(utxoAdapter_);

   walletName_ = QString::fromStdString(wallet_->name());
}

DealerCCSettlementContainer::~DealerCCSettlementContainer()
{
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

bs::sync::PasswordDialogData DealerCCSettlementContainer::toPasswordDialogData() const
{
   bs::sync::PasswordDialogData dialogData = SettlementContainer::toPasswordDialogData();
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

bool DealerCCSettlementContainer::startSigning()
{
   if (!wallet_) {
      logger_->error("[DealerCCSettlementContainer::accept] failed to validate counterparty's TX - aborting");
      emit failed();
      return false;
   }

   QPointer<DealerCCSettlementContainer> context(this);
   const auto &cbTx = [this, context, logger=logger_](bs::error::ErrorCode result, const BinaryData &signedTX) {
      QMetaObject::invokeMethod(qApp, [this, result, signedTX, context, logger] {
         if (!context) {
            logger->warn("[DealerCCSettlementContainer::onTXSigned] failed to sign TX half, already destroyed");
            return;
         }

         if (result == bs::error::ErrorCode::NoError) {
            emit signTxRequest(orderId_, signedTX.toHexStr());
            emit completed();
            wallet_->setTransactionComment(signedTX, txComment());
         }
         else if (result == bs::error::ErrorCode::TxCanceled) {
            // FIXME
            // Not clear what's wrong here, and what should be fixed
            emit failed();
         }
         else {
            logger->warn("[DealerCCSettlementContainer::onTXSigned] failed to sign TX half: {}", bs::error::ErrorCodeToString(result).toStdString());
            emit error(tr("TX half signing failed\n: %1").arg(bs::error::ErrorCodeToString(result)));
            emit failed();
         }
      });
   };

   txReq_.walletIds = { wallet_->walletId() };
   txReq_.prevStates = { txReqData_ };
   txReq_.populateUTXOs = true;
   txReq_.inputs = utxoAdapter_->get(id());
   logger_->debug("[DealerCCSettlementContainer::accept] signing with wallet {}, {} inputs"
      , wallet_->name(), txReq_.inputs.size());

   //Waiting for TX half signing...

   bs::signer::RequestId signId = signingContainer_->signSettlementPartialTXRequest(txReq_, toPasswordDialogData(), cbTx);
   return (signId > 0);
}

void DealerCCSettlementContainer::activate()
{
   try {
      signer_.deserializeState(txReqData_);
      foundRecipAddr_ = signer_.findRecipAddress(ownRecvAddr_, [this](uint64_t value, uint64_t valReturn, uint64_t valInput) {
         // Fix SIGFPE crash
         if (lotSize_ == 0) {
            return;
         }
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
      wallet_ = nullptr;
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
   startSigning();
}

void DealerCCSettlementContainer::onGenAddressVerified(bool addressVerified)
{
   genAddrVerified_ = addressVerified;
   if (addressVerified) {
      //Accept offer to send your own signed half of the CoinJoin transaction
      emit readyToAccept();
   }
   else {
      logger_->warn("[DealerCCSettlementContainer::onGenAddressVerified] counterparty's TX is unverified");
      emit error(tr("Failed to verify counterparty's transaction"));
      wallet_ = nullptr;
   }

   bs::sync::PasswordDialogData pd;
   pd.setValue(PasswordDialogData::SettlementId, id());
   pd.setValue(PasswordDialogData::DeliveryUTXOVerified, addressVerified);
   pd.setValue(PasswordDialogData::SigningAllowed, addressVerified);

   signingContainer_->updateDialogData(pd);
}

bool DealerCCSettlementContainer::cancel()
{
   utxoAdapter_->unreserve(id());
   signingContainer_->CancelSignTx(id());
   cancelled_ = true;
   return true;
}

QString DealerCCSettlementContainer::GetSigningWalletName() const
{
   return walletName_;
}

std::string DealerCCSettlementContainer::txComment()
{
   return std::string(bs::network::Side::toString(order_.side))
      + " " + order_.security + " @ " + std::to_string(order_.price);
}
