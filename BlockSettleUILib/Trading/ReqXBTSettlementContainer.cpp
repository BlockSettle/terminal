#include "ReqXBTSettlementContainer.h"

#include <QApplication>

#include <spdlog/spdlog.h>

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "CheckRecipSigner.h"
#include "CurrencyPair.h"
#include "QuoteProvider.h"
#include "SelectedTransactionInputs.h"
#include "SettlementMonitor.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <sstream>

using namespace bs::sync;

Q_DECLARE_METATYPE(AddressVerificationState)

ReqXBTSettlementContainer::ReqXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<AuthAddressManager> &authAddrMgr
   , const std::shared_ptr<SignContainer> &signContainer
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<Wallet> &xbtWallet
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const bs::network::RFQ &rfq
   , const bs::network::Quote &quote
   , const bs::Address &authAddr
   , const std::vector<UTXO> &utxosPayinFixed
   , const bs::Address &recvAddr)
   : bs::SettlementContainer()
   , logger_(logger)
   , authAddrMgr_(authAddrMgr)
   , walletsMgr_(walletsMgr)
   , signContainer_(signContainer)
   , armory_(armory)
   , xbtWallet_(xbtWallet)
   , rfq_(rfq)
   , quote_(quote)
   , recvAddr_(recvAddr)
   , clientSellsXbt_(!rfq.isXbtBuy())
   , authAddr_(authAddr)
   , utxosPayinFixed_(utxosPayinFixed)
{
   assert(authAddr.isValid());

   qRegisterMetaType<AddressVerificationState>();

   utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
   bs::UtxoReservation::addAdapter(utxoAdapter_);

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
   if (clientSellsXbt_) {
      utxoAdapter_->unreserve(id());
   }
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

void ReqXBTSettlementContainer::createPayoutTx(const BinaryData& payinHash, double qty
   , const bs::Address &recvAddr)
{

   usedPayinHash_ = payinHash;

   armory_->estimateFee(2, [this, qty, payinHash, recvAddr, handle = validityFlag_.handle()](float fee) {
      if (!handle.isValid()) {
         return;
      }
      auto feePerByte = ArmoryConnection::toFeePerByte(fee);
      if (feePerByte < 1.0f) {
         SPDLOG_LOGGER_ERROR(logger_, "wrong fee: {} s/b", feePerByte);
         cancelWithError(tr("Invalid fee"));
         return;
      }

      try {
         const auto txReq = bs::SettlementMonitor::createPayoutTXRequest(
            bs::SettlementMonitor::getInputFromTX(settlAddr_, payinHash, bs::XBTAmount{ qty }), recvAddr
            , feePerByte, armory_->topBlock());

         bs::sync::PasswordDialogData dlgData = toPayOutTxDetailsPasswordDialogData(txReq);
         dlgData.setValue(PasswordDialogData::Market, "XBT");
         dlgData.setValue(PasswordDialogData::SettlementId, settlementId_.toHexStr());
         dlgData.setValue(PasswordDialogData::ResponderAuthAddressVerified, true);
         dlgData.setValue(PasswordDialogData::SigningAllowed, true);

         logger_->debug("[ReqXBTSettlementContainer::createPayoutTx] pay-out fee={}, qty={} ({}), payin hash={}"
            , txReq.fee, qty, qty * BTCNumericTypes::BalanceDivider, payinHash.toHexStr(true));

         payoutSignId_ = signContainer_->signSettlementPayoutTXRequest(txReq
            , {settlementId_, dealerAuthKey_, true }, dlgData);
      }
      catch (const std::exception &e) {
         logger_->warn("[ReqXBTSettlementContainer::createPayoutTx] failed to create pay-out transaction based on {}: {}"
            , payinHash.toHexStr(), e.what());
         cancelWithError(tr("Pay-out transaction creation failure: %1").arg(QLatin1String(e.what())));
      }
   });
}

void ReqXBTSettlementContainer::acceptSpotXBT()
{
   emit acceptQuote(rfq_.requestId, "not used");
}

bool ReqXBTSettlementContainer::cancel()
{
   deactivate();
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

   settlementIdHex_ = quote_.settlementId;

   addrVerificator_ = std::make_shared<AddressVerificator>(logger_, armory_
      , [this, handle = validityFlag_.handle()](const bs::Address &address, AddressVerificationState state)
   {
      QMetaObject::invokeMethod(qApp, [this, handle, address, state] {
         if (!handle.isValid()) {
            return;
         }
         dealerAuthAddress_ = address;
         dealerVerifStateChanged(state);
      });
   });

   addrVerificator_->SetBSAddressList(authAddrMgr_->GetBSAddresses());

   settlementId_ = BinaryData::CreateFromHex(quote_.settlementId);
   userKey_ = BinaryData::CreateFromHex(quote_.requestorAuthPublicKey);
   dealerAuthKey_ = BinaryData::CreateFromHex(quote_.dealerAuthPublicKey);

   const auto priWallet = walletsMgr_->getPrimaryWallet();
   if (!priWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "missing primary wallet");
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

   settlLeaf->setSettlementID(settlementId_, [this, priWallet, handle = validityFlag_.handle()](bool success) {
      if (!handle.isValid()) {
         return;
      }

      if (!success) {
         SPDLOG_LOGGER_ERROR(logger_, "can't find settlement leaf for auth address '{}'"
            , authAddr_.display());
         return;
      }

      const auto &cbSettlAddr = [this](const bs::Address &addr) {
         settlAddr_ = addr;

         acceptSpotXBT();
      };

      priWallet->getSettlementPayinAddress(settlementId_, dealerAuthKey_, cbSettlAddr, !clientSellsXbt_);
   });
}

void ReqXBTSettlementContainer::deactivate()
{
   stopTimer();
}

bs::sync::PasswordDialogData ReqXBTSettlementContainer::toPasswordDialogData() const
{
   bs::sync::PasswordDialogData dialogData = SettlementContainer::toPasswordDialogData();
   dialogData.setValue(PasswordDialogData::Market, "XBT");
   dialogData.setValue(PasswordDialogData::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementRequestor));

   // rfq details
   QString qtyProd = UiUtils::XbtCurrency;
   QString fxProd = QString::fromStdString(fxProduct());

   dialogData.setValue(PasswordDialogData::Title, tr("Settlement Pay-In"));
   dialogData.setValue(PasswordDialogData::Price, UiUtils::displayPriceXBT(price()));
   dialogData.setValue(PasswordDialogData::FxProduct, fxProd);



   bool isFxProd = (quote_.product != bs::network::XbtCurrency);

   if (isFxProd) {
      dialogData.setValue(PasswordDialogData::Quantity, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(quantity(), fxProd, bs::network::Asset::Type::SpotXBT))
                    .arg(fxProd));

      dialogData.setValue(PasswordDialogData::TotalValue, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(quantity() / price())));
   }
   else {
      dialogData.setValue(PasswordDialogData::Quantity, tr("%1 XBT")
                    .arg(UiUtils::displayAmount(amount())));

      dialogData.setValue(PasswordDialogData::TotalValue, tr("%1 %2")
                    .arg(UiUtils::displayAmountForProduct(amount() * price(), fxProd, bs::network::Asset::Type::SpotXBT))
                    .arg(fxProd));
   }

   // settlement details
   dialogData.setValue(PasswordDialogData::SettlementId, settlementId_.toHexStr());
   dialogData.setValue(PasswordDialogData::SettlementAddress, settlAddr_.display());

   dialogData.setValue(PasswordDialogData::RequesterAuthAddress, authAddr_.display());
   dialogData.setValue(PasswordDialogData::RequesterAuthAddressVerified, true);

   dialogData.setValue(PasswordDialogData::ResponderAuthAddress, bs::Address::fromPubKey(dealerAuthKey_).display());
   dialogData.setValue(PasswordDialogData::ResponderAuthAddressVerified, dealerVerifState_ == AddressVerificationState::Verified);


   // tx details
   dialogData.setValue(PasswordDialogData::TxInputProduct, UiUtils::XbtCurrency);

   return dialogData;
}

void ReqXBTSettlementContainer::dealerVerifStateChanged(AddressVerificationState state)
{
   dealerVerifState_ = state;
   bs::sync::PasswordDialogData pd;
   pd.setValue(PasswordDialogData::SettlementId, settlementId_.toHexStr());
   pd.setValue(PasswordDialogData::ResponderAuthAddress, dealerAuthAddress_.display());
   pd.setValue(PasswordDialogData::ResponderAuthAddressVerified, state == AddressVerificationState::Verified);
   pd.setValue(PasswordDialogData::SigningAllowed, state == AddressVerificationState::Verified);
   signContainer_->updateDialogData(pd);
}

void ReqXBTSettlementContainer::cancelWithError(const QString& errorMessage)
{
   emit error(errorMessage);
   cancel();
}

void ReqXBTSettlementContainer::onTXSigned(unsigned int id, BinaryData signedTX
   , bs::error::ErrorCode errCode, std::string errTxt)
{
   if (payinSignId_ != 0 && (payinSignId_ == id)) {
      payinSignId_ = 0;

      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         cancelWithError(tr("Failed to create Pay-In TX - re-type password and try again"));
         logger_->error("[ReqXBTSettlementContainer::onTXSigned] Failed to create pay-in TX: {} ({})"
            , (int)errCode, errTxt);
         return;
      }

      emit sendSignedPayinToPB(settlementIdHex_, signedTX);

      xbtWallet_->setTransactionComment(signedTX, comment_);
//    walletsMgr_->getSettlementWallet()->setTransactionComment(signedTX, comment_);  //TODO: later

      // OK. if payin created - settletlement accepted for this RFQ
      deactivate();
      emit settlementAccepted();

   } else if (payoutSignId_ != 0 && (payoutSignId_ == id)) {
      payoutSignId_ = 0;

      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.isNull()) {
         logger_->warn("[ReqXBTSettlementContainer::onTXSigned] Pay-Out sign failure: {} ({})"
            , (int)errCode, errTxt);
         cancelWithError(tr("Pay-Out signing failed: %1").arg(QString::fromStdString(errTxt)));
         return;
      }

      logger_->debug("[ReqXBTSettlementContainer::onTXSigned] signed payout: {}"
                     , signedTX.toHexStr());

      try {
         Tx tx{signedTX};

         auto txdata = tx.serialize();
         auto bctx = BCTX::parse(txdata);

         auto utxo = bs::SettlementMonitor::getInputFromTX(settlAddr_, usedPayinHash_, bs::XBTAmount{ amount_ });

         std::map<BinaryData, std::map<unsigned, UTXO>> utxoMap;
         utxoMap[utxo.getTxHash()][0] = utxo;

         TransactionVerifier tsv(*bctx, utxoMap);

         auto tsvFlags = tsv.getFlags();
         tsvFlags |= SCRIPT_VERIFY_P2SH_SHA256 | SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_SEGWIT;
         tsv.setFlags(tsvFlags);

         auto verifierState = tsv.evaluateState();

         auto inputState = verifierState.getSignedStateForInput(0);

         auto signatureCount = inputState.getSigCount();

         if (signatureCount != 1) {
            logger_->error("[ReqXBTSettlementContainer::onTXSigned] signature count: {}", signatureCount);
            cancelWithError(tr("Failed to sign Pay-out"));
            return;
         }
      } catch (...) {
         logger_->error("[ReqXBTSettlementContainer::onTXSigned] failed to deserialize signed payout");
         cancelWithError(tr("Failed to sign Pay-out"));
         return;
      }

      emit sendSignedPayoutToPB(settlementIdHex_, signedTX);

      xbtWallet_->setTransactionComment(signedTX, comment_);
//         walletsMgr_->getSettlementWallet()->setTransactionComment(payoutData_, comment_); //TODO: later

      // OK. if payout created - settletlement accepted for this RFQ
      deactivate();
      emit settlementAccepted();
   }
}

void ReqXBTSettlementContainer::onUnsignedPayinRequested(const std::string& settlementId)
{
   if (settlementIdHex_ != settlementId) {
      logger_->error("[ReqXBTSettlementContainer::onUnsignedPayinRequested] invalid id : {} . {} expected"
                     , settlementId, settlementIdHex_);
      return;
   }

   if (!clientSellsXbt_) {
      logger_->error("[ReqXBTSettlementContainer::onUnsignedPayinRequested] customer buy on thq rfq {}. should not create unsigned payin"
                     , settlementId);
      return;
   }

   logger_->debug("[ReqXBTSettlementContainer::onUnsignedPayinRequested] unsigned payin requested: {}"
                  , settlementId);

   armory_->estimateFee(2, [this, handle = validityFlag_.handle()](float fee) {
      if (!handle.isValid()) {
         return;
      }
      auto feePerByte = ArmoryConnection::toFeePerByte(fee);
      if (feePerByte < 1.0f) {
         SPDLOG_LOGGER_ERROR(logger_, "wrong fee: {} s/b", feePerByte);
         cancelWithError(tr("Invalid fee"));
         return;
      }

      const auto changedCallback = nullptr;
      const bool isSegWitInputsOnly = true;
      const bool confirmedOnly = true;
      auto transaction = std::make_shared<TransactionData>(changedCallback, logger_, isSegWitInputsOnly, confirmedOnly);

      transaction->setFeePerByte(feePerByte);

      auto resetInputsCb = [this, transaction]{
         QMetaObject::invokeMethod(qApp, [this, transaction] {
            auto index = transaction->RegisterNewRecipient();
            assert(index == 0);
            transaction->UpdateRecipient(0, amount_, settlAddr_);

            const auto list = authAddrMgr_->GetVerifiedAddressList();
            const auto userAddress = bs::Address::fromPubKey(userKey_, AddressEntryType_P2WPKH);
            userKeyOk_ = (std::find(list.begin(), list.end(), userAddress) != list.end());
            if (!userKeyOk_) {
               SPDLOG_LOGGER_WARN(logger_, "userAddr {} not found in verified addrs list ({})"
                  , userAddress.display(), list.size());
               return;
            }

            if (!transaction->IsTransactionValid()) {
               userKeyOk_ = false;
               logger_->error("[ReqXBTSettlementContainer::activate] transaction data is invalid");
               cancelWithError(tr("Transaction data is invalid - sending of pay-in is prohibited"));
               return;
            }

            const auto dealerAddrSW = bs::Address::fromPubKey(dealerAuthKey_, AddressEntryType_P2WPKH);
            addrVerificator_->addAddress(dealerAddrSW);
            addrVerificator_->startAddressVerification();

            const auto &cbChangeAddr = [this, transaction](const bs::Address &changeAddr) {
               unsignedPayinRequest_ = transaction->createUnsignedTransaction(false, changeAddr);

               if (!unsignedPayinRequest_.isValid()) {
                  SPDLOG_LOGGER_ERROR(logger_, "unsigned payin request is invalid: {}", settlementIdHex_);
                  return;
               }

               const auto cbPreimage = [this, transaction](const std::map<bs::Address, BinaryData> &preimages)
               {
                  const auto resolver = bs::sync::WalletsManager::getPublicResolver(preimages);

                  const auto unsignedTxId = unsignedPayinRequest_.txId(resolver);

                  SPDLOG_LOGGER_DEBUG(logger_, "unsigned tx id {}", unsignedTxId.toHexStr(true));

                  utxoAdapter_->reserve(xbtWallet_->walletId(), id(), unsignedPayinRequest_.inputs);

                  emit sendUnsignedPayinToPB(settlementIdHex_, unsignedPayinRequest_.serializeState(resolver), unsignedTxId);
               };

               std::map<std::string, std::vector<bs::Address>> addrMapping;
               const auto wallet = transaction->getWallet();
               const auto walletId = wallet->walletId();

               for (const auto &utxo : transaction->inputs()) {
                  const auto addr = bs::Address::fromUTXO(utxo);
                  addrMapping[walletId].push_back(addr);
               }

               signContainer_->getAddressPreimage(addrMapping, cbPreimage);
            };

            xbtWallet_->getNewChangeAddress(cbChangeAddr);
         });
      };

      if (utxosPayinFixed_.empty()) {
         transaction->setWallet(xbtWallet_, armory_->topBlock(), false, resetInputsCb);
      } else {
         transaction->setWalletAndInputs(xbtWallet_, utxosPayinFixed_, armory_->topBlock());
         transaction->getSelectedInputs()->SetUseAutoSel(true);
         resetInputsCb();
      }
   });
}

void ReqXBTSettlementContainer::onSignedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash)
{
   if (settlementIdHex_ != settlementId) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayoutRequested] invalid id : {} . {} expected"
                     , settlementId, settlementIdHex_);
      return;
   }

   logger_->debug("[ReqXBTSettlementContainer::onSignedPayoutRequested] create payout for {} on {} for {}"
                  , settlementId, payinHash.toHexStr(), amount_);

   auto recvAddressCb = [this, payinHash, handle = validityFlag_.handle()](const bs::Address &addr) {
      if (!handle.isValid()) {
         return;
      }
      createPayoutTx(payinHash, amount_, addr);
   };

   if (recvAddr_.isNull()) {
      xbtWallet_->getNewExtAddress(recvAddressCb);
   } else {
      recvAddressCb(recvAddr_);
   }
}

void ReqXBTSettlementContainer::onSignedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin)
{
   if (settlementIdHex_ != settlementId) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayinRequested] invalid id : {} . {} expected"
                     , settlementId, settlementIdHex_);
      return;
   }

   if (!clientSellsXbt_) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayinRequested] customer buy on thq rfq {}. should not sign payin"
                     , settlementId);
      return;
   }

   if (!unsignedPayinRequest_.isValid()) {
      logger_->error("[ReqXBTSettlementContainer::onSignedPayinRequested] unsigned payin request is invalid: {}"
                     , settlementIdHex_);
      return;
   }

   logger_->debug("[ReqXBTSettlementContainer::onSignedPayinRequested] signed payout requested {}"
                  , settlementId);

   // XXX check unsigned payin?

   bs::sync::PasswordDialogData dlgData = toPasswordDialogData();
   dlgData.setValue(PasswordDialogData::SettlementPayInVisible, true);

   payinSignId_ = signContainer_->signSettlementTXRequest(unsignedPayinRequest_, dlgData);
}
