#include "ReqXBTSettlementContainer.h"

#include <QApplication>
#include <QPointer>

#include <spdlog/spdlog.h>

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "CheckRecipSigner.h"
#include "CurrencyPair.h"
#include "QuoteProvider.h"
#include "SettlementMonitor.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

static const unsigned int kWaitTimeoutInSec = 30;

using namespace bs::sync::dialog;

Q_DECLARE_METATYPE(AddressVerificationState)

ReqXBTSettlementContainer::ReqXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<AuthAddressManager> &authAddrMgr
   , const std::shared_ptr<SignContainer> &signContainer
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const bs::network::RFQ &rfq
   , const bs::network::Quote &quote
   , const std::shared_ptr<TransactionData> &txData
   , const bs::Address &authAddr)
   : bs::SettlementContainer()
   , logger_(logger)
   , authAddrMgr_(authAddrMgr)
   , walletsMgr_(walletsMgr)
   , signContainer_(signContainer)
   , armory_(armory)
   , transactionData_(txData)
   , rfq_(rfq)
   , quote_(quote)
   , clientSells_(!rfq.isXbtBuy())
   , authAddr_(authAddr)
{
   assert(authAddr.isValid());

   qRegisterMetaType<AddressVerificationState>();

   utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
   bs::UtxoReservation::addAdapter(utxoAdapter_);

   connect(signContainer_.get(), &SignContainer::QWalletInfo, this, &ReqXBTSettlementContainer::onWalletInfo);
   connect(signContainer_.get(), &SignContainer::TXSigned, this, &ReqXBTSettlementContainer::onTXSigned);

   connect(this, &ReqXBTSettlementContainer::timerExpired, this, &ReqXBTSettlementContainer::onTimerExpired);

   CurrencyPair cp(quote_.security);
   const bool isFxProd = (quote_.product != bs::network::XbtCurrency);
   fxProd_ = cp.ContraCurrency(bs::network::XbtCurrency);
   amount_ = isFxProd ? quantity() / price() : quantity();

   comment_ = std::string(bs::network::Side::toString(bs::network::Side::invert(quote_.side))) + " "
      + quote_.security + " @ " + std::to_string(price());

}

ReqXBTSettlementContainer::~ReqXBTSettlementContainer()
{
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

unsigned int ReqXBTSettlementContainer::createPayoutTx(const BinaryData& payinHash, double qty
   , const bs::Address &recvAddr)
{
   try {
      const auto txReq = bs::SettlementMonitor::createPayoutTXRequest(
         bs::SettlementMonitor::getInputFromTX(settlAddr_, payinHash, qty), recvAddr
         , transactionData_->GetTransactionSummary().feePerByte, armory_->topBlock());

      bs::sync::PasswordDialogData dlgData = toPayOutTxDetailsPasswordDialogData(txReq);
      dlgData.setValue(keys::SettlementId, QString::fromStdString(settlementId_.toHexStr()));
      dlgData.setValue(keys::ResponderAuthAddressVerified, true);
      dlgData.setValue(keys::SigningAllowed, true);

      // mark revoke tx
      if ((side() == bs::network::Side::Type::Sell && product() == bs::network::XbtCurrency)
          || (side() == bs::network::Side::Type::Buy && product() != bs::network::XbtCurrency)) {
         dlgData.setValue(keys::PayOutRevokeType, true);
         dlgData.setValue(keys::Title, tr("Settlement Pay-Out (For Revoke)"));
         dlgData.setValue(keys::Duration, 30000 - (QDateTime::currentMSecsSinceEpoch() - payinSignedTs_));
      }

      logger_->debug("[ReqXBTSettlementContainer::createPayoutTx] pay-out fee={}, qty={} ({}), payin hash={}"
         , txReq.fee, qty, qty * BTCNumericTypes::BalanceDivider, payinHash.toHexStr(true));

      return signContainer_->signSettlementPayoutTXRequest(txReq
         , {settlementId_, dealerAuthKey_, !clientSells_ }, dlgData);
   }
   catch (const std::exception &e) {
      logger_->warn("[ReqXBTSettlementContainer::createPayoutTx] failed to create pay-out transaction based on {}: {}"
         , payinHash.toHexStr(), e.what());
      emit error(tr("Pay-out transaction creation failure: %1").arg(QLatin1String(e.what())));
   }
   return 0;
}

void ReqXBTSettlementContainer::acceptSpotXBT()
{
   emit acceptQuote(rfq_.requestId, "not used");
}

bool ReqXBTSettlementContainer::cancel()
{
   deactivate();
   if (clientSells_) {
      utxoAdapter_->unreserve(id());
   }
   emit settlementCancelled();
   return true;
}

void ReqXBTSettlementContainer::onTimerExpired()
{
   cancel();
}

void ReqXBTSettlementContainer::activate()
{
   startTimer(kWaitTimeoutInSec);

   const auto &authWallet = walletsMgr_->getAuthWallet();
   auto rootAuthWallet = walletsMgr_->getHDRootForLeaf(authWallet->walletId());

   walletInfoAuth_.setName(rootAuthWallet->name());
   walletInfoAuth_.setRootId(rootAuthWallet->walletId());

   walletInfo_.setRootId(walletsMgr_->getHDRootForLeaf(transactionData_->getWallet()->walletId())->walletId());

   if (clientSells_) {
      sellFromPrimary_ = (walletInfoAuth_.rootId() == walletInfo_.rootId());

      if (!sellFromPrimary_) {
         infoReqIdAuth_ = signContainer_->GetInfo(rootAuthWallet->walletId());
      }
   }

   infoReqId_ = signContainer_->GetInfo(walletInfo_.rootId().toStdString());

   QPointer<ReqXBTSettlementContainer> thisPtr = this;
   addrVerificator_ = std::make_shared<AddressVerificator>(logger_, armory_
      , [thisPtr](const bs::Address &address, AddressVerificationState state)
   {
      QMetaObject::invokeMethod(qApp, [thisPtr, address, state] {
         if (!thisPtr) {
            return;
         }
         thisPtr->dealerAuthAddress_ = address;
         thisPtr->dealerVerifStateChanged(state);
      });
   });
   addrVerificator_->SetBSAddressList(authAddrMgr_->GetBSAddresses());

   settlementIdString_ = quote_.settlementId;
   settlementId_ = BinaryData::CreateFromHex(quote_.settlementId);
   userKey_ = BinaryData::CreateFromHex(quote_.requestorAuthPublicKey);
   dealerAuthKey_ = BinaryData::CreateFromHex(quote_.dealerAuthPublicKey);

   const auto priWallet = walletsMgr_->getPrimaryWallet();
   if (!priWallet) {
      logger_->error("[ReqXBTSettlementContainer::activate] missing primary wallet");
      return;
   }

   const auto group = std::dynamic_pointer_cast<bs::sync::hd::SettlementGroup>(priWallet->getGroup(bs::hd::BlockSettle_Settlement));
   if (!group) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement group");
      return;
   }

   auto settlLeaf = group->getLeaf(authAddr_);
   if (!settlLeaf) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf for auth address '{}'", authAddr_.display());
      return;
   }

   settlLeaf->setSettlementID(settlementId_, [thisPtr](bool success) {
      if (!thisPtr) {
         return;
      }

      if (!success) {
         SPDLOG_LOGGER_ERROR(thisPtr->logger_, "can't find settlement leaf for auth address '{}'", thisPtr->authAddr_.display());
         return;
      }

      thisPtr->activateProceed();
   });
}

void ReqXBTSettlementContainer::deactivate()
{
   stopTimer();
}

bs::sync::PasswordDialogData ReqXBTSettlementContainer::toPasswordDialogData() const
{
   bs::sync::PasswordDialogData dialogData = SettlementContainer::toPasswordDialogData();
   dialogData.setValue(keys::Market, "XBT");
   dialogData.setValue(keys::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementRequestor));

   // rfq details
   QString qtyProd = UiUtils::XbtCurrency;
   QString fxProd = QString::fromStdString(fxProduct());

   dialogData.setValue(keys::Title, tr("Settlement Pay-In"));
   dialogData.setValue(keys::Price, UiUtils::displayPriceXBT(price()));
   dialogData.setValue(keys::FxProduct, fxProd);



   bool isFxProd = (quote_.product != bs::network::XbtCurrency);

   if (isFxProd) {
      dialogData.setValue(keys::Quantity, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(quantity(), fxProd, bs::network::Asset::Type::SpotXBT))
                    .arg(fxProd));

      dialogData.setValue(keys::TotalValue, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(quantity() / price())));
   }
   else {
      dialogData.setValue(keys::Quantity, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(amount())));

      dialogData.setValue(keys::TotalValue, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(amount() * price(), fxProd, bs::network::Asset::Type::SpotXBT))
                    .arg(fxProd));
   }

   // settlement details
   dialogData.setValue(keys::SettlementId, settlementId_.toHexStr());
   dialogData.setValue(keys::SettlementAddress, settlAddr_.display());

   dialogData.setValue(keys::RequesterAuthAddress, authAddr_.display());
   dialogData.setValue(keys::RequesterAuthAddressVerified, true);

   dialogData.setValue(keys::ResponderAuthAddress, bs::Address::fromPubKey(dealerAuthKey_).display());
   dialogData.setValue(keys::ResponderAuthAddressVerified, false);


   // tx details
   dialogData.setValue(keys::TxInputProduct, UiUtils::XbtCurrency);
   dialogData.setValue(keys::TotalSpentVisible, true);

   return dialogData;
}

void ReqXBTSettlementContainer::dealerVerifStateChanged(AddressVerificationState state)
{
   bs::sync::PasswordDialogData pd;
   pd.setValue(keys::ResponderAuthAddress, dealerAuthAddress_.display());
   pd.setValue(keys::ResponderAuthAddressVerified, state == AddressVerificationState::Verified);
   pd.setValue(keys::SigningAllowed, state == AddressVerificationState::Verified);
   signContainer_->updateDialogData(pd);
}

void ReqXBTSettlementContainer::activateProceed()
{
   const auto &cbSettlAddr = [this](const bs::Address &addr) {
      settlAddr_ = addr;
      const auto &buyAuthKey = clientSells_ ? dealerAuthKey_ : userKey_;
      const auto &sellAuthKey = clientSells_ ? userKey_ : dealerAuthKey_;

      recvAddr_ = transactionData_->GetFallbackRecvAddress();

      const auto recipient = transactionData_->RegisterNewRecipient();
      transactionData_->UpdateRecipientAmount(recipient, amount_, transactionData_->maxSpendAmount());
      transactionData_->UpdateRecipientAddress(recipient, settlAddr_);

      const auto list = authAddrMgr_->GetVerifiedAddressList();
      const auto userAddress = bs::Address::fromPubKey(userKey_, AddressEntryType_P2WPKH);
      userKeyOk_ = (std::find(list.begin(), list.end(), userAddress) != list.end());
      if (!userKeyOk_) {
         logger_->warn("[ReqXBTSettlementContainer::activate] userAddr {} not found in verified addrs list ({})"
            , userAddress.display(), list.size());
         return;
      }

      if (clientSells_) {
         if (!transactionData_->IsTransactionValid()) {
            userKeyOk_ = false;
            logger_->error("[ReqXBTSettlementContainer::activate] transaction data is invalid");
            emit error(tr("Transaction data is invalid - sending of pay-in is prohibited"));
            return;
         }
      }

      fee_ = transactionData_->GetTransactionSummary().totalFee;

      const auto dealerAddrSW = bs::Address::fromPubKey(dealerAuthKey_, AddressEntryType_P2WPKH);
      addrVerificator_->addAddress(dealerAddrSW);
      addrVerificator_->startAddressVerification();

      acceptSpotXBT();
   };

   const auto priWallet = walletsMgr_->getPrimaryWallet();
   priWallet->getSettlementPayinAddress(settlementId_, dealerAuthKey_, cbSettlAddr, !clientSells_);
}

void ReqXBTSettlementContainer::onTXSigned(unsigned int id, BinaryData signedTX
   , bs::error::ErrorCode errCode, std::string errTxt)
{
   if (payinSignId_ && (payinSignId_ == id)) {
      payinSignId_ = 0;

      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         emit error(tr("Failed to create Pay-In TX - re-type password and try again"));
         logger_->error("[ReqXBTSettlementContainer::onTXSigned] Failed to create pay-in TX: {} ({})"
            , (int)errCode, errTxt);
         emit retry();
         return;
      }

      emit sendSignedPayinToPB(settlementIdString_, signedTX);

      transactionData_->getWallet()->setTransactionComment(signedTX, comment_);
//    walletsMgr_->getSettlementWallet()->setTransactionComment(signedTX, comment_);  //TODO: later

      // OK. if payin created - settletlement accepted for this RFQ
      deactivate();
      emit settlementAccepted();

   } else if (payoutSignId_ && (payoutSignId_ == id)) {
      payoutSignId_ = 0;

      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         logger_->warn("[ReqXBTSettlementContainer::onTXSigned] Pay-Out sign failure: {} ({})"
            , (int)errCode, errTxt);
         emit error(tr("Pay-Out signing failed: %1").arg(QString::fromStdString(errTxt)));
         emit retry();
         return;
      }

      if (!clientSells_) {
         transactionData_->getWallet()->setTransactionComment(signedTX, comment_);
//         walletsMgr_->getSettlementWallet()->setTransactionComment(payoutData_, comment_); //TODO: later
      }

      emit sendSignedPayoutToPB(settlementIdString_, signedTX);

      // OK. if payout created - settletlement accepted for this RFQ
      deactivate();
      emit settlementAccepted();
   }
}

void ReqXBTSettlementContainer::onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo &walletInfo)
{
   if (infoReqId_ && (reqId == infoReqId_)) {
      infoReqId_ = 0;
      walletInfo_.setEncKeys(walletInfo.encKeys());
      walletInfo_.setEncTypes(walletInfo.encTypes());
      walletInfo_.setKeyRank(walletInfo.keyRank());
   }
   if (infoReqIdAuth_ && (reqId == infoReqIdAuth_)) {
      infoReqIdAuth_ = 0;
      walletInfoAuth_.setEncKeys(walletInfo.encKeys());
      walletInfoAuth_.setEncTypes(walletInfo.encTypes());
      walletInfoAuth_.setKeyRank(walletInfo.keyRank());
      emit authWalletInfoReceived();
   }
}

void ReqXBTSettlementContainer::onUnsignedPayinRequested(const std::string& settlementId)
{
   if (settlementIdString_ != settlementId) {
      logger_->error("[ReqXBTSettlementContainer::onUnsignedPayinRequested] invalid id : {} . {} expected"
                     , settlementId, settlementIdString_);
      return;
   }

   if (!clientSells_) {
      logger_->error("[ReqXBTSettlementContainer::onUnsignedPayinRequested] customer buy on thq rfq {}. should not create unsigned payin"
                     , settlementId);
      return;
   }

   logger_->debug("[ReqXBTSettlementContainer::onUnsignedPayinRequested] unsigned payout requested: {}"
                  , settlementId);

   const auto &cbChangeAddr = [this](const bs::Address &changeAddr) {
      unsignedPayinRequest_ = transactionData_->createUnsignedTransaction(false, changeAddr);

      if (!unsignedPayinRequest_.isValid()) {
         logger_->error("[ReqXBTSettlementContainer::onUnsignedPayinRequested cb] unsigned payin request is invalid: {}"
                        , settlementIdString_);
         return;
      }

      // XXX: make reservation on UTXO

      emit sendUnsignedPayinToPB(settlementIdString_, unsignedPayinRequest_.serializeState());
   };

   if (transactionData_->GetTransactionSummary().hasChange) {
      transactionData_->getWallet()->getNewChangeAddress(cbChangeAddr);
   }
   else {
      cbChangeAddr({});
   }
}

void ReqXBTSettlementContainer::onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash)
{
   if (settlementIdString_ != settlementId) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayoutRequested] invalid id : {} . {} expected"
                     , settlementId, settlementIdString_);
      return;
   }

   logger_->debug("[ReqXBTSettlementContainer::onSignedPayoutRequested] create payout for {} on {} for {}"
                  , settlementId, payinHash.toHexStr(), amount_);

   payoutSignId_ = createPayoutTx(payinHash, amount_, recvAddr_);
}

void ReqXBTSettlementContainer::onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin)
{
   if (settlementIdString_ != settlementId) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayinRequested] invalid id : {} . {} expected"
                     , settlementId, settlementIdString_);
      return;
   }

   if (!clientSells_) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayinRequested] customer buy on thq rfq {}. should not sign payin"
                     , settlementId);
      return;
   }

   if (!unsignedPayinRequest_.isValid()) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayinRequested] unsigned payin request is invalid: {}"
                     , settlementIdString_);
      return;
   }

   logger_->debug("[ReqXBTSettlementContainer::onSignedPayinRequested] signed payout requested {}"
                  , settlementId);

   // XXX check unsigned payin?

   bs::sync::PasswordDialogData dlgData = toPasswordDialogData();
   dlgData.setValue(keys::SettlementPayInVisible, true);

   payinSignedTs_ = QDateTime::currentMSecsSinceEpoch();
   payinSignId_ = signContainer_->signSettlementTXRequest(unsignedPayinRequest_, dlgData);
}
