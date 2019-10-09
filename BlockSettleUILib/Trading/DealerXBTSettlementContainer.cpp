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

using namespace bs::sync;

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

   addrVerificator_ = std::make_shared<AddressVerificator>(logger, armory
      , [logger, this, handle = validityFlag_.handle()](const bs::Address &address, AddressVerificationState state)
   {
      QMetaObject::invokeMethod(qApp, [this, handle, address, state] {
         if (!handle.isValid()) {
            return;
         }

         logger_->info("Counterparty's address verification {} for {}"
            , to_string(state), address.display());
         requestorAddressState_ = state;

         if (state == AddressVerificationState::Verified) {
            // we verify only requester's auth address
            bs::sync::PasswordDialogData dialogData;
            dialogData.setValue(PasswordDialogData::RequesterAuthAddressVerified, true);
            dialogData.setValue(PasswordDialogData::SettlementId, QString::fromStdString(id()));
            dialogData.setValue(PasswordDialogData::SigningAllowed, true);

            signContainer_->updateDialogData(dialogData);
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
   dialogData.setValue(PasswordDialogData::Market, "XBT");
   dialogData.setValue(PasswordDialogData::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementDealer));

   // rfq details
   QString qtyProd = UiUtils::XbtCurrency;

   dialogData.setValue(PasswordDialogData::Title, tr("Settlement Pay-In"));
   dialogData.setValue(PasswordDialogData::Price, UiUtils::displayPriceXBT(price()));
   dialogData.setValue(PasswordDialogData::FxProduct, fxProd_);

   bool isFxProd = (product() != bs::network::XbtCurrency);

   if (isFxProd) {
      dialogData.setValue(PasswordDialogData::Quantity, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(quantity(), QString::fromStdString(fxProd_), bs::network::Asset::Type::SpotXBT))
                    .arg(QString::fromStdString(fxProd_)));

      dialogData.setValue(PasswordDialogData::TotalValue, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(quantity() / price())));
   }
   else {
      dialogData.setValue(PasswordDialogData::Quantity, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(amount())));

      dialogData.setValue(PasswordDialogData::TotalValue, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(amount() * price(), QString::fromStdString(fxProd_), bs::network::Asset::Type::SpotXBT))
                    .arg(QString::fromStdString(fxProd_)));
   }

   // settlement details
   dialogData.setValue(PasswordDialogData::SettlementId, settlementId_.toHexStr());
   dialogData.setValue(PasswordDialogData::SettlementAddress, settlAddr_.display());

   dialogData.setValue(PasswordDialogData::RequesterAuthAddress, bs::Address::fromPubKey(reqAuthKey_).display());
   dialogData.setValue(PasswordDialogData::RequesterAuthAddressVerified, requestorAddressState_ == AddressVerificationState::Verified);

   dialogData.setValue(PasswordDialogData::ResponderAuthAddress, bs::Address::fromPubKey(authKey_).display());
   dialogData.setValue(PasswordDialogData::ResponderAuthAddressVerified, true);

   // tx details
   dialogData.setValue(PasswordDialogData::TxInputProduct, UiUtils::XbtCurrency);

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
      dlgData.setValue(PasswordDialogData::SettlementPayInVisible, true);

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
   logger_->debug("[DealerXBTSettlementContainer::startPayOutSigning] create payout for {} on {} for {}"
                  , settlementIdString_, payinHash.toHexStr(), amount_);

   const auto &txWallet = transactionData_->getWallet();
   if (txWallet->type() != bs::core::wallet::Type::Bitcoin) {
      logger_->error("[DealerSettlDialog::onAccepted] Invalid payout wallet type: {}", (int)txWallet->type());
      emit error(tr("Invalid payout wallet type"));
      emit failed();
      return false;
   }

   auto fallbackAddrCb = [this, handle = validityFlag_.handle(), txWallet, payinHash](const bs::Address &receivingAddress)
   {
      if (!handle.isValid()) {
         return;
      }
      if (!txWallet->containsAddress(receivingAddress)) {
         logger_->error("[DealerSettlDialog::onAccepted] Invalid receiving address");
         emit error(tr("Invalid receiving address"));
         emit failed();
         return;
      }

      try {
         auto payOutTxRequest = bs::SettlementMonitor::createPayoutTXRequest(
            bs::SettlementMonitor::getInputFromTX(settlAddr_, payinHash, amount_), receivingAddress
            , transactionData_->feePerByte(), armory_->topBlock());

         bs::sync::PasswordDialogData dlgData = toPayOutTxDetailsPasswordDialogData(payOutTxRequest);
         dlgData.setValue(PasswordDialogData::Market, "XBT");
         dlgData.setValue(PasswordDialogData::SettlementId, settlementId_.toHexStr());
         dlgData.setValue(PasswordDialogData::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementDealer));

         payoutSignId_ = signContainer_->signSettlementPayoutTXRequest(payOutTxRequest
            , { settlementId_, reqAuthKey_, true }
            , dlgData);
      } catch (const std::exception &e) {
         logger_->error("[DealerSettlDialog::onAccepted] Failed to sign pay-out: {}", e.what());
         emit error(tr("Failed to sign pay-out"));
         emit failed();

         return;
      }
   };
   transactionData_->GetFallbackRecvAddress(fallbackAddrCb);
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

      const auto &fallbackRecvAddress = transactionData_->GetFallbackRecvAddressIfSet();
      assert(fallbackRecvAddress.isValid());
      txWallet->setAddressComment(fallbackRecvAddress
         , bs::sync::wallet::Comment::toString(bs::sync::wallet::Comment::SettlementPayOut));

      logger_->debug("[DealerXBTSettlementContainer::onTXSigned] signed payout: {}"
                     , signedTX.toHexStr());

      try {
         Tx tx{signedTX};

         std::stringstream ss;
         tx.pprintAlot(ss);
         logger_->debug("[DealerXBTSettlementContainer::onTXSigned] info on signed payout:\n{}"
                        , ss.str());
      } catch (...) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] failed to deserialize signed payout");
      }


      emit sendSignedPayoutToPB(settlementIdString_, signedTX);

      // ok. there is nothing this container could/should do
      emit completed();
   } else if (payinSignId_ && (payinSignId_ == id)) {
      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] Failed to sign pay-in: {} ({})", (int)errCode, errMsg);
         emit error(tr("Failed to sign pay-in"));
         emit failed();
         return;
      }

      transactionData_->getWallet()->setTransactionComment(signedTX, comment_);
//      settlWallet_->setTransactionComment(signedTX, comment_);  //TODO: implement later

      try {
         Tx tx{signedTX};

         std::stringstream ss;

         tx.pprintAlot(ss);

         logger_->debug("[DealerXBTSettlementContainer::onTXSigned] info on signed payin:\n{}"
                        , ss.str());
      } catch (...) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] failed to deserialize signed payin");
      }

      emit sendSignedPayinToPB(settlementIdString_, signedTX);
      logger_->debug("[DealerXBTSettlementContainer::onTXSigned] Payin sent");

      // ok. there is nothing this container could/should do
      emit completed();
   }
}

void DealerXBTSettlementContainer::onUnsignedPayinRequested(const std::string& settlementId)
{
   if (settlementIdString_ != settlementId) {
      // ignore
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

      const auto cbPreimage = [this](const std::map<bs::Address, BinaryData> &preimages)
      {
         const auto resolver = bs::sync::WalletsManager::getPublicResolver(preimages);

         const auto unsignedTxId = unsignedPayinRequest_.txId(resolver);

         logger_->debug("[DealerXBTSettlementContainer::onUnsignedPayinRequested cbPreimage] unsigned tx id {}", unsignedTxId.toHexStr());

         emit sendUnsignedPayinToPB(settlementIdString_, unsignedPayinRequest_.serializeState(resolver), unsignedTxId);
      };

      std::map<std::string, std::vector<bs::Address>> addrMapping;
      const auto wallet = transactionData_->getWallet();
      const auto walletId = wallet->walletId();

      for (const auto &utxo : transactionData_->inputs()) {
         const auto addr = bs::Address::fromUTXO(utxo);
         addrMapping[walletId].push_back(addr);
      }

      signContainer_->getAddressPreimage(addrMapping, cbPreimage);
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
      // ignore
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
      // ignore
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
