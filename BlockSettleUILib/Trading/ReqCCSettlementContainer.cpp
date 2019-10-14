#include "ReqCCSettlementContainer.h"
#include <spdlog/spdlog.h>
#include "AssetManager.h"
#include "CheckRecipSigner.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UtxoReservation.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "BSErrorCodeStrings.h"
#include "UiUtils.h"
#include "XBTAmount.h"

using namespace bs::sync;

ReqCCSettlementContainer::ReqCCSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<AssetManager> &assetMgr
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const bs::network::RFQ &rfq, const bs::network::Quote &quote
   , const std::shared_ptr<TransactionData> &txData)
   : bs::SettlementContainer(), logger_(logger), signingContainer_(container)
   , transactionData_(txData)
   , assetMgr_(assetMgr)
   , walletsMgr_(walletsMgr)
   , rfq_(rfq)
   , quote_(quote)
   , genAddress_(assetMgr_->getCCGenesisAddr(product()))
   , dealerAddress_(quote_.dealerAuthPublicKey)
   , signer_(armory)
   , lotSize_(assetMgr_->getCCLotSize(product()))
{
   utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
   bs::UtxoReservation::addAdapter(utxoAdapter_);

   connect(signingContainer_.get(), &SignContainer::QWalletInfo, this, &ReqCCSettlementContainer::onWalletInfo);
   connect(this, &ReqCCSettlementContainer::genAddressVerified, this
      , &ReqCCSettlementContainer::onGenAddressVerified, Qt::QueuedConnection);

   const auto &signingWallet = transactionData_->getSigningWallet();
   if (signingWallet) {
      const auto &rootWallet = walletsMgr_->getHDRootForLeaf(signingWallet->walletId());
      walletInfo_ = bs::hd::WalletInfo(walletsMgr_, rootWallet);
      infoReqId_ = signingContainer_->GetInfo(walletInfo_.rootId().toStdString());
   }
   else {
      throw std::runtime_error("missing signing wallet");
   }

   dealerTx_ = BinaryData::CreateFromHex(quote_.dealerTransaction);
   requesterTx_ = BinaryData::CreateFromHex(rfq_.coinTxInput);
}

ReqCCSettlementContainer::~ReqCCSettlementContainer()
{
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

bs::sync::PasswordDialogData ReqCCSettlementContainer::toPasswordDialogData() const
{
   bs::sync::PasswordDialogData dialogData = SettlementContainer::toPasswordDialogData();
   dialogData.setValue(PasswordDialogData::Market, "CC");
   dialogData.setValue(PasswordDialogData::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementRequestor));
   dialogData.setValue(PasswordDialogData::LotSize, qint64(lotSize_));

   dialogData.remove(PasswordDialogData::SettlementId);

   if (side() == bs::network::Side::Sell) {
      dialogData.setValue(PasswordDialogData::Title, tr("Settlement Delivery"));
   }
   else {
      dialogData.setValue(PasswordDialogData::Title, tr("Settlement Payment"));
   }

   // rfq details
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

void ReqCCSettlementContainer::activate()
{
   if (side() == bs::network::Side::Buy) {
      if (amount() > assetMgr_->getBalance(bs::network::XbtCurrency, transactionData_->getSigningWallet())) {
         emit paymentVerified(false, tr("Insufficient XBT balance in signing wallet"));
         return;
      }
   }

   startTimer(kWaitTimeoutInSec);

   userKeyOk_ = false;
   bool foundRecipAddr = false;
   bool amountValid = false;
   try {
      if (!lotSize_) {
         throw std::runtime_error("invalid lot size");
      }
      signer_.deserializeState(dealerTx_);
      foundRecipAddr = signer_.findRecipAddress(bs::Address(rfq_.receiptAddress)
         , [this, &amountValid](uint64_t value, uint64_t valReturn, uint64_t valInput) {
         if ((quote_.side == bs::network::Side::Sell) && qFuzzyCompare(quantity(), value / lotSize_)) {
            amountValid = valInput == (value + valReturn);
         }
         else if (quote_.side == bs::network::Side::Buy) {
            const auto quoteVal = static_cast<uint64_t>(amount() * BTCNumericTypes::BalanceDivider);
            const auto diff = (quoteVal > value) ? quoteVal - value : value - quoteVal;
            if (diff < 3) {
               amountValid = valInput > (value + valReturn);
            }
         }
      });
   }
   catch (const std::exception &e) {
      logger_->debug("Signer deser exc: {}", e.what());
      emit error(tr("Failed to verify dealer's TX: %1").arg(QLatin1String(e.what())));
   }

   emit paymentVerified(foundRecipAddr && amountValid, QString{});

   if (genAddress_.isNull()) {
      emit genAddressVerified(false, tr("GA is null"));
   }
   else if (side() == bs::network::Side::Buy) {
      //Waiting for genesis address verification to complete...

      const auto &cbHasInput = [this](bool has) {
         userKeyOk_ = has;
         emit genAddressVerified(has, has ? QString{} : tr("GA check failed"));
      };
      signer_.hasInputAddress(genAddress_, cbHasInput, lotSize_);
   }
   else {
      userKeyOk_ = true;
      emit genAddressVerified(true, QString{});
   }

   if (!createCCUnsignedTXdata()) {
      userKeyOk_ = false;
      emit error(tr("Failed to create unsigned CC transaction"));
   }
}

void ReqCCSettlementContainer::deactivate()
{
   stopTimer();
}

// KLUDGE currently this code not just making unsigned TX, but also initiate signing
bool ReqCCSettlementContainer::createCCUnsignedTXdata()
{
   const auto wallet = transactionData_->getSigningWallet();
   if (!wallet) {
      logger_->error("[{}] failed to get signing wallet", __func__);
      return false;
   }

   if (side() == bs::network::Side::Sell) {
      const uint64_t spendVal = quantity() * assetMgr_->getCCLotSize(product());
      logger_->debug("[{}] sell amount={}, spend value = {}", __func__, quantity(), spendVal);
      ccTxData_.walletIds = { wallet->walletId() };
      ccTxData_.prevStates = { dealerTx_ };
      const auto recipient = bs::Address(dealerAddress_).getRecipient(bs::XBTAmount{ spendVal });
      if (recipient) {
         ccTxData_.recipients.push_back(recipient);
      }
      else {
         logger_->error("[{}] failed to create recipient from {} and value {}"
            , __func__, dealerAddress_, spendVal);
         return false;
      }
      ccTxData_.populateUTXOs = true;
      ccTxData_.inputs = utxoAdapter_->get(id());
      logger_->debug("[{}] {} CC inputs reserved ({} recipients)"
         , __func__, ccTxData_.inputs.size(), ccTxData_.recipients.size());

      // KLUDGE - in current implementation, we should sign first to have sell/buy process aligned
      startSigning();
   }
   else {
      const auto &cbFee = [this](float feePerByte) {
         const uint64_t spendVal = amount() * BTCNumericTypes::BalanceDivider;
         const auto &cbTxOutList = [this, feePerByte, spendVal](std::vector<UTXO> utxos) {
            try {
               const auto recipient = bs::Address(dealerAddress_).getRecipient(bs::XBTAmount{ spendVal });
               if (!recipient) {
                  logger_->error("[{}] invalid recipient: {}", __func__, dealerAddress_);
                  return;
               }
               ccTxData_ = transactionData_->createPartialTXRequest(spendVal, feePerByte, { recipient }
               , dealerTx_, utxos);
               logger_->debug("{} inputs in ccTxData", ccTxData_.inputs.size());
               utxoAdapter_->reserve(ccTxData_.walletIds.front(), id(), ccTxData_.inputs);

               startSigning();
            }
            catch (const std::exception &e) {
               logger_->error("[{}] Failed to create partial CC TX to {}: {}"
                  , __func__, dealerAddress_, e.what());
               QMetaObject::invokeMethod(this, [this] { emit error(tr("Failed to create CC TX half")); });
            }
         };
         if (!transactionData_->getWallet()->getSpendableTxOutList(cbTxOutList, spendVal)) {
            logger_->error("[{}] getSpendableTxOutList failed", __func__);
         }
      };
      walletsMgr_->estimatedFeePerByte(0, cbFee, this);
   }

   return true;
}

bool ReqCCSettlementContainer::startSigning()
{
   if (side() == bs::network::Side::Sell) {
      if (!ccTxData_.isValid()) {
         logger_->error("[CCSettlementTransactionWidget::createCCSignedTXdata] CC TX half wasn't created properly");
         emit error(tr("Failed to create TX half"));
         return false;
      }
   }

   emit sendOrder();

   QPointer<ReqCCSettlementContainer> context(this);
   const auto &cbTx = [this, context, logger=logger_](bs::error::ErrorCode result, const BinaryData &signedTX) {
      if (!context) {
         logger->warn("[ReqCCSettlementContainer::onTXSigned] failed to sign TX half, already destroyed");
         return;
      }

      if (result == bs::error::ErrorCode::NoError) {
         ccTxSigned_ = signedTX.toHexStr();

         // notify RFQ dialog that signed half could be saved
         emit settlementAccepted();
         transactionData_->getWallet()->setTransactionComment(signedTX, txComment());
      }
      else if (result == bs::error::ErrorCode::TxCanceled) {
         emit settlementCancelled();
      }
      else {
         logger->warn("[CCSettlementTransactionWidget::onTXSigned] CC TX sign failure: {}", bs::error::ErrorCodeToString(result).toStdString());
         emit error(tr("own TX half signing failed\n: %1").arg(bs::error::ErrorCodeToString(result)));
      }
   };

   ccSignId_ = signingContainer_->signSettlementPartialTXRequest(ccTxData_, toPasswordDialogData(), cbTx);
   logger_->debug("[CCSettlementTransactionWidget::createCCSignedTXdata] {} recipients", ccTxData_.recipients.size());
   return (ccSignId_ > 0);
}

std::string ReqCCSettlementContainer::txComment()
{
   return std::string(bs::network::Side::toString(bs::network::Side::invert(quote_.side))) + " "
      + quote_.security + " @ " + std::to_string(price());
}

void ReqCCSettlementContainer::onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo &walletInfo)
{
   if (!infoReqId_ || (reqId != infoReqId_)) {
      return;
   }

   // just update walletInfo_  to save walletName and id
   walletInfo_.setEncKeys(walletInfo.encKeys());
   walletInfo_.setEncTypes(walletInfo.encTypes());
   walletInfo_.setKeyRank(walletInfo.keyRank());

   emit walletInfoReceived();
}

void ReqCCSettlementContainer::onGenAddressVerified(bool addressVerified, const QString &error)
{
   genAddrVerified_ = addressVerified;

   bs::sync::PasswordDialogData pd;
   pd.setValue(PasswordDialogData::DeliveryUTXOVerified, addressVerified);
   pd.setValue(PasswordDialogData::SigningAllowed, addressVerified);
   signingContainer_->updateDialogData(pd);
}

bool ReqCCSettlementContainer::cancel()
{
   deactivate();
   utxoAdapter_->unreserve(id());
   emit settlementCancelled();
   signingContainer_->CancelSignTx(id());
   return true;
}

std::string ReqCCSettlementContainer::txData() const
{
   const auto &data = ccTxData_.serializeState().toHexStr();
   logger_->debug("[ReqCCSettlementContainer::txData] {}", data);
   return data;
}
