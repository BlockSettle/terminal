#include "DealerXBTSettlementContainer.h"

#include "AuthAddressManager.h"
#include "CheckRecipSigner.h"
#include "CurrencyPair.h"
#include "QuoteProvider.h"
#include "SettlementMonitor.h"
#include "SignContainer.h"
#include "TradesUtils.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <spdlog/spdlog.h>

#include <QApplication>

Q_DECLARE_METATYPE(AddressVerificationState)

using namespace bs::sync;

DealerXBTSettlementContainer::DealerXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
   , const bs::network::Order &order
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<Wallet> &xbtWallet
   , const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<AuthAddressManager> &authAddrMgr
   , const bs::Address &authAddr
   , const std::vector<UTXO> &utxosPayinFixed
   , const bs::Address &recvAddr)
   : bs::SettlementContainer()
   , order_(order)
   , weSellXbt_((order.side == bs::network::Side::Buy) != (order.product == bs::network::XbtCurrency))
   , amount_((order.product != bs::network::XbtCurrency) ? order.quantity / order.price : order.quantity)
   , logger_(logger)
   , armory_(armory)
   , walletsMgr_(walletsMgr)
   , xbtWallet_(xbtWallet)
   , signContainer_(container)
   , authAddrMgr_(authAddrMgr)
   , utxosPayinFixed_(utxosPayinFixed)
   , recvAddr_(recvAddr)
   , authAddr_(authAddr)
{
   qRegisterMetaType<AddressVerificationState>();

   utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
   bs::UtxoReservation::addAdapter(utxoAdapter_);

   CurrencyPair cp(security());
   fxProd_ = cp.ContraCurrency(bs::network::XbtCurrency);

   if (!xbtWallet_) {
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

   settlementIdHex_ = qn.settlementId;
   settlementId_ = BinaryData::CreateFromHex(qn.settlementId);

   connect(signContainer_.get(), &SignContainer::TXSigned, this, &DealerXBTSettlementContainer::onTXSigned);
}

DealerXBTSettlementContainer::~DealerXBTSettlementContainer()
{
   if (weSellXbt_) {
      utxoAdapter_->unreserve(id());
   }
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

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

bool DealerXBTSettlementContainer::cancel()
{
   return true;
}

void DealerXBTSettlementContainer::activate()
{
   startTimer(kWaitTimeoutInSec);

   addrVerificator_ = std::make_shared<AddressVerificator>(logger_, armory_
      , [this, handle = validityFlag_.handle()](const bs::Address &address, AddressVerificationState state)
   {
      QMetaObject::invokeMethod(qApp, [this, handle, address, state] {
         if (!handle.isValid()) {
            return;
         }

         SPDLOG_LOGGER_INFO(logger_, "counterparty's address verification {} for {}"
            , to_string(state), address.display());
         requestorAddressState_ = state;

         if (state == AddressVerificationState::Verified) {
            // we verify only requester's auth address
            bs::sync::PasswordDialogData dialogData;
            dialogData.setValue(PasswordDialogData::RequesterAuthAddressVerified, true);
            dialogData.setValue(PasswordDialogData::SettlementId, settlementId_.toHexStr());
            dialogData.setValue(PasswordDialogData::SigningAllowed, true);

            signContainer_->updateDialogData(dialogData);
         }
      });
   });

   addrVerificator_->SetBSAddressList(authAddrMgr_->GetBSAddresses());

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
         SPDLOG_LOGGER_ERROR(logger_, "failed to sign pay-out: {} ({})", int(errCode), errMsg);
         failWithErrorText(tr("Failed to sign pay-out"));
         return;
      }

      bs::tradeutils::PayoutVerifyArgs verifyArgs;
      verifyArgs.signedTx = signedTX;
      verifyArgs.settlAddr = settlAddr_;
      verifyArgs.usedPayinHash = usedPayinHash_;
      verifyArgs.amount = bs::XBTAmount(amount_);
      auto verifyResult = bs::tradeutils::verifySignedPayout(verifyArgs);
      if (!verifyResult.success) {
         SPDLOG_LOGGER_ERROR(logger_, "payout verification failed: {}", verifyResult.errorMsg);
         failWithErrorText(tr("payin verification failed"));
         return;
      }

      xbtWallet_->setTransactionComment(signedTX, comment_);
//      settlWallet_->setTransactionComment(signedTX, comment_);   //TODO: implement later

      SPDLOG_LOGGER_DEBUG(logger_, "signed payout: {}", signedTX.toHexStr());

      emit sendSignedPayoutToPB(settlementIdHex_, signedTX);

      // ok. there is nothing this container could/should do
      emit completed();
   }

   if ((payinSignId_ != 0) && (payinSignId_ == id)) {
      payinSignId_ = 0;

      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         SPDLOG_LOGGER_ERROR(logger_, "Failed to sign pay-in: {} ({})", (int)errCode, errMsg);
         failWithErrorText(tr("Failed to sign Pay-in"));
         return;
      }

      xbtWallet_->setTransactionComment(signedTX, comment_);
//      settlWallet_->setTransactionComment(signedTX, comment_);  //TODO: implement later

      emit sendSignedPayinToPB(settlementIdHex_, signedTX);
      logger_->debug("[DealerXBTSettlementContainer::onTXSigned] Payin sent");

      // ok. there is nothing this container could/should do
      emit completed();
   }
}

void DealerXBTSettlementContainer::onUnsignedPayinRequested(const std::string& settlementId)
{
   if (settlementIdHex_ != settlementId) {
      // ignore
      return;
   }

   if (!weSellXbt_) {
      SPDLOG_LOGGER_ERROR(logger_, "dealer is buying. Should not create unsigned payin on {}", settlementIdHex_);
      return;
   }

   bs::tradeutils::PayinArgs args;
   initTradesArgs(args, settlementId);
   args.fixedInputs = utxosPayinFixed_;
   args.inputXbtWallets.push_back(xbtWallet_);
   args.utxoReservation = bs::UtxoReservation::instance();
   args.utxoReservationWalletId = xbtWallet_->walletId();

   auto payinCb = bs::tradeutils::PayinResultCb([this, handle = validityFlag_.handle()]
      (bs::tradeutils::PayinResult result)
   {
      QMetaObject::invokeMethod(qApp, [this, handle, result = std::move(result)] {
         if (!handle.isValid()) {
            return;
         }

         if (!result.success) {
            SPDLOG_LOGGER_ERROR(logger_, "creating payin request failed: {}", result.errorMsg);
            failWithErrorText(tr("creating payin request failed"));
            return;
         }

         settlAddr_ = result.settlementAddr;

         unsignedPayinRequest_ = std::move(result.signRequest);
         SPDLOG_LOGGER_DEBUG(logger_, "unsigned tx id {}", result.payinTxId.toHexStr(true));

         utxoAdapter_->reserve(xbtWallet_->walletId(), id(), unsignedPayinRequest_.inputs);

         emit sendUnsignedPayinToPB(settlementIdHex_, unsignedPayinRequest_.serializeState(), result.payinTxId);
      });
   });

   bs::tradeutils::createPayin(std::move(args), std::move(payinCb));
}

void DealerXBTSettlementContainer::onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash)
{
   if (settlementIdHex_ != settlementId) {
      // ignore
      return;
   }

   startTimer(kWaitTimeoutInSec);

   if (weSellXbt_) {
      SPDLOG_LOGGER_ERROR(logger_, "dealer is selling. Should not sign payout on {}", settlementIdHex_);
      return;
   }

   usedPayinHash_ = payinHash;

   bs::tradeutils::PayoutArgs args;
   initTradesArgs(args, settlementId);
   args.payinTxId = payinHash;
   args.recvAddr = recvAddr_;
   args.outputXbtWallet = xbtWallet_;

   auto payoutCb = bs::tradeutils::PayoutResultCb([this, payinHash, handle = validityFlag_.handle()]
      (bs::tradeutils::PayoutResult result)
   {
      QMetaObject::invokeMethod(qApp, [this, payinHash, handle, result = std::move(result)] {
         if (!handle.isValid()) {
            return;
         }

         if (!result.success) {
            SPDLOG_LOGGER_ERROR(logger_, "creating payout failed: {}", result.errorMsg);
            failWithErrorText(tr("creating payout failed"));
            return;
         }

         settlAddr_ = result.settlementAddr;

         bs::sync::PasswordDialogData dlgData = toPayOutTxDetailsPasswordDialogData(result.signRequest);
         dlgData.setValue(PasswordDialogData::Market, "XBT");
         dlgData.setValue(PasswordDialogData::SettlementId, settlementId_.toHexStr());
         dlgData.setValue(PasswordDialogData::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementDealer));

         payoutSignId_ = signContainer_->signSettlementPayoutTXRequest(result.signRequest
            , { settlementId_, reqAuthKey_, true }, dlgData);
      });
   });
   bs::tradeutils::createPayout(std::move(args), std::move(payoutCb));
}

void DealerXBTSettlementContainer::onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin)
{
   if (settlementIdHex_ != settlementId) {
      // ignore
      return;
   }

   startTimer(kWaitTimeoutInSec);

   SPDLOG_LOGGER_DEBUG(logger_, "start sign payin: {}", settlementId);

   if (!weSellXbt_) {
      SPDLOG_LOGGER_ERROR(logger_, "dealer is buying. Should not sign payin on {}", settlementIdHex_);
      return;
   }

   if (!unsignedPayinRequest_.isValid()) {
      SPDLOG_LOGGER_ERROR(logger_, "unsigned payin request is invalid: {}", settlementIdHex_);
      failWithErrorText(tr("Failed to sign pay-in"));
      return;
   }

   bs::sync::PasswordDialogData dlgData = toPasswordDialogData();
   dlgData.setValue(PasswordDialogData::SettlementPayInVisible, true);

   payinSignId_ = signContainer_->signSettlementTXRequest(unsignedPayinRequest_, dlgData, SignContainer::TXSignMode::Full);
}

void DealerXBTSettlementContainer::failWithErrorText(const QString& errorMessage)
{
   emit error(errorMessage);
   emit failed();
}

void DealerXBTSettlementContainer::initTradesArgs(bs::tradeutils::Args &args, const std::string &settlementId)
{
   args.amount = bs::XBTAmount{amount_};
   args.settlementId = BinaryData::CreateFromHex(settlementId);
   args.ourAuthAddress = authAddr_;
   args.cpAuthPubKey = reqAuthKey_;
   args.walletsMgr = walletsMgr_;
   args.armory = armory_;
   args.signContainer = signContainer_;
}
