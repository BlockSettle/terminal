#include "DealerCCSettlementContainer.h"

#include <spdlog/spdlog.h>

#include "BSErrorCodeStrings.h"
#include "CheckRecipSigner.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"
#include "SignerDefs.h"

#include <QPointer>

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
   , ownRecvAddr_(ownRecvAddr)
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
   dialogData.setValue("AutoSignCategory", static_cast<int>(bs::signer::AutoSignCategory::SettlementDealer));

   dialogData.remove("SettlementId");

   if (side() == bs::network::Side::Sell) {
      dialogData.setValue("Title", tr("Settlement Delivery"));
   }
   else {
      dialogData.setValue("Title", tr("Settlement Payment"));
   }

   // rfq details
   QString qtyProd = UiUtils::XbtCurrency;

   dialogData.setValue("Price", UiUtils::displayPriceCC(price()));
   dialogData.setValue("Quantity", tr("%1 %2")
                 .arg(UiUtils::displayCCAmount(quantity()))
                 .arg(QString::fromStdString(product())));
   dialogData.setValue("TotalValue", UiUtils::displayAmount(quantity() * price()));

   // tx details
   if (side() == bs::network::Side::Buy) {
      dialogData.setValue("InputAmount", QStringLiteral("- %2 %1")
                    .arg(UiUtils::XbtCurrency)
                    .arg(UiUtils::displayAmount(txReq_.inputAmount())));

      dialogData.setValue("ReturnAmount", QStringLiteral("+ %2 %1")
                    .arg(UiUtils::XbtCurrency)
                    .arg(UiUtils::displayAmount(txReq_.change.value)));

      dialogData.setValue("PaymentAmount", QStringLiteral("- %2 %1")
                    .arg(UiUtils::XbtCurrency)
                    .arg(UiUtils::displayAmount(txReq_.inputAmount() - txReq_.change.value)));

      dialogData.setValue("DeliveryReceived", QStringLiteral("+ %2 %1")
                    .arg(QString::fromStdString(product()))
                    .arg(UiUtils::displayCCAmount(txReq_.change.value / lotSize_)));
   }
   else {
      dialogData.setValue("InputAmount", QStringLiteral("- %2 %1")
                    .arg(QString::fromStdString(product()))
                    .arg(UiUtils::displayCCAmount(txReq_.inputAmount() / lotSize_)));

      dialogData.setValue("ReturnAmount", QStringLiteral("+ %2 %1")
                    .arg(QString::fromStdString(product()))
                    .arg(UiUtils::displayCCAmount(txReq_.change.value / lotSize_)));

      dialogData.setValue("DeliveryAmount", QStringLiteral("- %2 %1")
                    .arg(QString::fromStdString(product()))
                    .arg(UiUtils::displayCCAmount((txReq_.inputAmount() - txReq_.change.value) / lotSize_)));

      dialogData.setValue("PaymentReceived", QStringLiteral("+ %2 %1")
                    .arg(UiUtils::XbtCurrency)
                    .arg(UiUtils::displayAmount(amount())));
   }

   // settlement details
   dialogData.setValue("DeliveryUTXOVerified", genAddrVerified_);
   dialogData.setValue("SigningAllowed", genAddrVerified_);

   dialogData.setValue("RecipientsList", true);
   dialogData.setValue("InputsList", true);

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
      if (!context) {
         logger->warn("[DealerCCSettlementContainer::onTXSigned] failed to sign TX half, already destroyed");
         return;
      }

      if (result == bs::error::ErrorCode::NoError) {
         emit signTxRequest(orderId_, signedTX.toHexStr());
         emit completed();
      }
      else if (result == bs::error::ErrorCode::TxCanceled) {
         // FIXME
         emit failed();
      }
      else {
         logger->warn("[DealerCCSettlementContainer::onTXSigned] failed to sign TX half: {}", bs::error::ErrorCodeToString(result).toStdString());
         emit error(tr("TX half signing failed\n: %1").arg(bs::error::ErrorCodeToString(result)));
         emit failed();
      }
   };

   txReq_.walletId = wallet_->walletId();
   txReq_.prevStates = { txReqData_ };
   txReq_.populateUTXOs = true;
   txReq_.inputs = utxoAdapter_->get(id());
   logger_->debug("[DealerCCSettlementContainer::accept] signing with wallet {}, {} inputs"
      , wallet_->name(), txReq_.inputs.size());

   emit info(tr("Waiting for TX half signing..."));

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
            amountValid_ = valInput > (value + valReturn);
         }
      });
   }
   catch (const std::exception &e) {
      logger_->error("Signer deser exc: {}", e.what());
      emit genAddressVerified(false);
      return;
   }

   if (!foundRecipAddr_ || !amountValid_) {
      logger_->warn("[DealerCCSettlementContainer::activate] requester's TX verification failed");
      wallet_ = nullptr;
      emit genAddressVerified(false);
   }
   else if (order_.side == bs::network::Side::Buy) {
      emit info(tr("Waiting for genesis address verification to complete..."));
      const auto &cbHasInput = [this](bool has) {
         emit genAddressVerified(has);
      };
      signer_.hasInputAddress(genesisAddr_, cbHasInput, lotSize_);
   }
   else {
      emit genAddressVerified(true);
   }

   startTimer(30);
   startSigning();
}

void DealerCCSettlementContainer::onGenAddressVerified(bool addressVerified)
{
   genAddrVerified_ = addressVerified;
   if (addressVerified) {
      emit info(tr("Accept offer to send your own signed half of the CoinJoin transaction"));
      emit readyToAccept();
   }
   else {
      logger_->warn("[DealerCCSettlementContainer::onGenAddressVerified] counterparty's TX is unverified");
      emit error(tr("Failed to verify counterparty's transaction"));
      wallet_ = nullptr;
   }

   bs::sync::PasswordDialogData pd;
   pd.setValue("DeliveryUTXOVerified", addressVerified);
   pd.setValue("SigningAllowed", addressVerified);
   signingContainer_->updateDialogData(pd);
}

bool DealerCCSettlementContainer::isAcceptable() const
{
   return (foundRecipAddr_ && amountValid_ && genAddrVerified_ && wallet_);
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
