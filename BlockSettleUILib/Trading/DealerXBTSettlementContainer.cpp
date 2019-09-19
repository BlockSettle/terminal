#include "DealerXBTSettlementContainer.h"

#include "CheckRecipSigner.h"
#include "CurrencyPair.h"
#include "QuoteProvider.h"
#include "SettlementMonitor.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <spdlog/spdlog.h>

#include <QApplication>

Q_DECLARE_METATYPE(AddressVerificationState)

using namespace bs::sync::dialog;

DealerXBTSettlementContainer::DealerXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
   , const bs::network::Order &order, const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<QuoteProvider> &quoteProvider, const std::shared_ptr<TransactionData> &txData
   , const std::unordered_set<std::string> &bsAddresses, const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory)
   : bs::SettlementContainer(), armory_(armory), walletsMgr_(walletsMgr), order_(order)
   , weSell_((order.side == bs::network::Side::Buy) ^ (order.product == bs::network::XbtCurrency))
   , amount_((order.product != bs::network::XbtCurrency) ? order.quantity / order.price : order.quantity)
   , logger_(logger), transactionData_(txData), signContainer_(container)
{
   qRegisterMetaType<AddressVerificationState>();

   CurrencyPair cp(security());
   fxProd_ = cp.ContraCurrency(bs::network::XbtCurrency);

   if (weSell_ && !transactionData_->GetRecipientsCount()) {
      throw std::runtime_error("no recipient[s]");
   }
   if (transactionData_->getWallet() == nullptr) {
      throw std::runtime_error("no wallet");
   }

   if (weSell_) {
      const Tx tx(BinaryData::CreateFromHex(order_.reqTransaction));
      if (!tx.isInitialized()) {
         throw std::runtime_error("no requester transaction");
      }
      const bs::TxChecker txChecker(tx);
      if ((tx.getNumTxIn() != 1) || !txChecker.hasInput(BinaryData::CreateFromHex(order_.dealerTransaction))) {
         throw std::runtime_error("invalid payout spender");
      }
   }

   auto qn = quoteProvider->getSubmittedXBTQuoteNotification(order.settlementId);
   if (qn.authKey.empty() || qn.reqAuthKey.empty() || qn.settlementId.empty()) {
      throw std::invalid_argument("failed to get submitted QN for " + order.quoteId);
   }

   comment_ = std::string(bs::network::Side::toString(order.side)) + " " + order.security + " @ " + std::to_string(order.price);
   authKey_ = BinaryData::CreateFromHex(qn.authKey);
   reqAuthKey_ = BinaryData::CreateFromHex(qn.reqAuthKey);
   if (authKey_.isNull() || reqAuthKey_.isNull()) {
      throw std::runtime_error("missing auth key");
   }

   settlementIdString_ = qn.settlementId;
   settlementId_ = BinaryData::CreateFromHex(qn.settlementId);

   QPointer<DealerXBTSettlementContainer> thisPtr = this;
   addrVerificator_ = std::make_shared<AddressVerificator>(logger, armory
      , [logger, thisPtr](const bs::Address &address, AddressVerificationState state)
   {
      QMetaObject::invokeMethod(qApp, [thisPtr, address, state] {
         if (!thisPtr) {
            return;
         }

         thisPtr->logger_->info("Counterparty's address verification {} for {}"
            , to_string(state), address.display());
         thisPtr->cptyAddressState_ = state;

         if (state == AddressVerificationState::Verified) {
            // we verify only requester's auth address
            bs::sync::PasswordDialogData dialogData;
            dialogData.setValue(keys::RequesterAuthAddressVerified, true);
            dialogData.setValue(keys::SettlementId, QString::fromStdString(thisPtr->id()));
            dialogData.setValue(keys::SigningAllowed, true);

            thisPtr->signContainer_->updateDialogData(dialogData);
         }
      });
   });

   const auto &cbSettlAddr = [this, bsAddresses](const bs::Address &addr) {
      settlAddr_ = addr;
      addrVerificator_->SetBSAddressList(bsAddresses);

      emit readyToActivate();
   };

   const auto priWallet = walletsMgr->getPrimaryWallet();
   if (!priWallet) {
      throw std::runtime_error("missing primary wallet");
   }
   priWallet->getSettlementPayinAddress(settlementId_, reqAuthKey_, cbSettlAddr, !weSell_);

   connect(signContainer_.get(), &SignContainer::TXSigned, this, &DealerXBTSettlementContainer::onTXSigned);
}

DealerXBTSettlementContainer::~DealerXBTSettlementContainer() = default;

bs::sync::PasswordDialogData DealerXBTSettlementContainer::toPasswordDialogData() const
{
   bs::sync::PasswordDialogData dialogData = SettlementContainer::toPasswordDialogData();
   dialogData.setValue(keys::Market, "XBT");
   dialogData.setValue(keys::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementDealer));

   // rfq details
   QString qtyProd = UiUtils::XbtCurrency;

   dialogData.setValue(keys::Title, tr("Settlement Pay-In"));
   dialogData.setValue(keys::Price, UiUtils::displayPriceXBT(price()));
   dialogData.setValue(keys::FxProduct, fxProd_);

   bool isFxProd = (product() != bs::network::XbtCurrency);

   if (isFxProd) {
      dialogData.setValue(keys::Quantity, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(quantity(), QString::fromStdString(fxProd_), bs::network::Asset::Type::SpotXBT))
                    .arg(QString::fromStdString(fxProd_)));

      dialogData.setValue(keys::TotalValue, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(quantity() / price())));
   }
   else {
      dialogData.setValue(keys::Quantity, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(amount())));

      dialogData.setValue(keys::TotalValue, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(amount() * price(), QString::fromStdString(fxProd_), bs::network::Asset::Type::SpotXBT))
                    .arg(QString::fromStdString(fxProd_)));
   }

   // settlement details
   dialogData.setValue(keys::SettlementId, settlementId_.toHexStr());
   dialogData.setValue(keys::SettlementAddress, settlAddr_.display());

   dialogData.setValue(keys::RequesterAuthAddress, bs::Address::fromPubKey(reqAuthKey_).display());
   dialogData.setValue(keys::RequesterAuthAddressVerified, false);

   dialogData.setValue(keys::ResponderAuthAddress, bs::Address::fromPubKey(authKey_).display());
   dialogData.setValue(keys::ResponderAuthAddressVerified, true);

   // tx details
   dialogData.setValue(keys::TxInputProduct, UiUtils::XbtCurrency);
   dialogData.setValue(keys::TotalSpentVisible, true);

   return dialogData;
}

bool DealerXBTSettlementContainer::startPayInSigning()
{
   try {
      if (!unsignedPayinRequest_.isValid()) {
         logger_->error("[DealerXBTSettlementContainer::startPayInSigning] unsigned payin request is invalid: {}"
                        , settlementIdString_);
         emit error(tr("Failed to sign pay-in"));
         emit failed();
         return false;
      }

      bs::sync::PasswordDialogData dlgData = toPasswordDialogData();
      dlgData.setValue(keys::SettlementPayInVisible, true);

      payinSignId_ = signContainer_->signSettlementTXRequest(unsignedPayinRequest_, dlgData, SignContainer::TXSignMode::Full);
   }
   catch (const std::exception &e) {
      logger_->error("[DealerXBTSettlementContainer::onAccepted] Failed to sign pay-in: {}", e.what());
      emit error(tr("Failed to sign pay-in"));
      emit failed();
      return false;
   }
   return true;
}

bool DealerXBTSettlementContainer::startPayOutSigning(const BinaryData& payinHash)
{
   const auto &txWallet = transactionData_->getWallet();
   if (txWallet->type() != bs::core::wallet::Type::Bitcoin) {
      logger_->error("[DealerSettlDialog::onAccepted] Invalid payout wallet type: {}", (int)txWallet->type());
      emit error(tr("Invalid payout wallet type"));
      emit failed();
      return false;
   }
   const auto receivingAddress = transactionData_->GetFallbackRecvAddress();
   if (!txWallet->containsAddress(receivingAddress)) {
      logger_->error("[DealerSettlDialog::onAccepted] Invalid receiving address");
      emit error(tr("Invalid receiving address"));
      emit failed();
      return false;
   }

   try {
      auto payOutTxRequest = bs::SettlementMonitor::createPayoutTXRequest(
         bs::SettlementMonitor::getInputFromTX(settlAddr_, payinHash, amount_), receivingAddress
         , transactionData_->GetTransactionSummary().feePerByte, armory_->topBlock());

      bs::sync::PasswordDialogData dlgData = toPayOutTxDetailsPasswordDialogData(payOutTxRequest);
      dlgData.setValue(keys::Market, "XBT");
      dlgData.setValue(keys::SettlementId, settlementId_.toHexStr());
      dlgData.setValue(keys::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementDealer));

      payoutSignId_ = signContainer_->signSettlementPayoutTXRequest(payOutTxRequest
         , { settlementId_, reqAuthKey_, !weSell_ }
         , dlgData);
   } catch (const std::exception &e) {
      logger_->error("[DealerSettlDialog::onAccepted] Failed to sign pay-out: {}", e.what());
      emit error(tr("Failed to sign pay-out"));
      emit failed();

      return false;
   }

   return true;
}

bool DealerXBTSettlementContainer::cancel()
{
   return true;
}

void DealerXBTSettlementContainer::activate()
{
   startTimer(30);

   const auto reqAuthAddrSW = bs::Address::fromPubKey(reqAuthKey_, AddressEntryType_P2WPKH);
   addrVerificator_->addAddress(reqAuthAddrSW);
   addrVerificator_->startAddressVerification();
}

void DealerXBTSettlementContainer::deactivate()
{
   stopTimer();
}

void DealerXBTSettlementContainer::onTXSigned(unsigned int id, BinaryData signedTX
   , bs::error::ErrorCode errCode, std::string errMsg)
{
   if (payoutSignId_ && (payoutSignId_ == id)) {
      payoutSignId_ = 0;
      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] Failed to sign pay-out: {} ({})", (int)errCode, errMsg);
         emit error(tr("Failed to sign pay-out"));
         emit failed();
         return;
      }

      const auto &txWallet = transactionData_->getWallet();
      txWallet->setTransactionComment(signedTX, comment_);
//      settlWallet_->setTransactionComment(signedTX, comment_);   //TODO: implement later
      txWallet->setAddressComment(transactionData_->GetFallbackRecvAddress()
         , bs::sync::wallet::Comment::toString(bs::sync::wallet::Comment::SettlementPayOut));

      emit sendSignedPayoutToPB(settlementIdString_, signedTX);
      logger_->debug("[DealerXBTSettlementContainer::onTXSigned] Payout sent");
   } else if (payinSignId_ && (payinSignId_ == id)) {
      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] Failed to sign pay-in: {} ({})", (int)errCode, errMsg);
         emit error(tr("Failed to sign pay-in"));
         emit failed();
         return;
      }

      transactionData_->getWallet()->setTransactionComment(signedTX, comment_);
//      settlWallet_->setTransactionComment(signedTX, comment_);  //TODO: implement later
      emit sendSignedPayinToPB(settlementIdString_, signedTX);

      logger_->debug("[DealerXBTSettlementContainer::onTXSigned] Payin sent");
   }
}

void DealerXBTSettlementContainer::onUnsignedPayinRequested(const std::string& settlementId)
{
   if (settlementIdString_ != settlementId) {
      logger_->error("[DealerXBTSettlementContainer::onUnsignedPayinRequested] settlement id mismatch {}. expected {}"
                     , settlementId, settlementIdString_);
      return;
   }

   if (!weSell_) {
      logger_->error("[DealerXBTSettlementContainer::onUnsignedPayinRequested] dealer is buying. Should not create unsigned payin on {}"
                     , settlementIdString_);
      return;
   }

   const auto &cbChangeAddr = [this](const bs::Address &changeAddr) {
      unsignedPayinRequest_ = transactionData_->createUnsignedTransaction(false, changeAddr);

      if (!unsignedPayinRequest_.isValid()) {
         logger_->error("[DealerXBTSettlementContainer::onUnsignedPayinRequested cb] unsigned payin request is invalid: {}"
                        , settlementIdString_);
         return;
      }

      emit sendUnsignedPayinToPB(settlementIdString_, unsignedPayinRequest_.serializeState());
   };

   if (transactionData_->GetTransactionSummary().hasChange) {
      transactionData_->getWallet()->getNewChangeAddress(cbChangeAddr);
   }
   else {
      cbChangeAddr({});
   }
}

void DealerXBTSettlementContainer::onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash)
{
   if (settlementIdString_ != settlementId) {
      logger_->error("[DealerXBTSettlementContainer::onSignedPayoutRequested] settlement id mismatch {}. expected {}"
                     , settlementId, settlementIdString_);
      return;
   }

   if (weSell_) {
      logger_->error("[DealerXBTSettlementContainer::onSignedPayoutRequested] dealer is selling. Should not sign payout on {}"
                     , settlementIdString_);
      return;
   }

   startPayOutSigning(payinHash);
}

void DealerXBTSettlementContainer::onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin)
{
   if (settlementIdString_ != settlementId) {
      logger_->error("[DealerXBTSettlementContainer::onSignedPayinRequested] settlement id mismatch {}. expected {}"
                     , settlementId, settlementIdString_);
      return;
   }

   if (!weSell_) {
      logger_->error("[DealerXBTSettlementContainer::onUnsignedPayinRequested] dealer is buying. Should not sign payin on {}"
                     , settlementIdString_);
      return;
   }

   logger_->debug("[DealerXBTSettlementContainer::onUnsignedPayinRequested] start sign payin: {}"
                  , settlementId);

   startPayInSigning();
}
