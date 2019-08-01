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

static const unsigned int kWaitTimeoutInSec = 30;

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
{
   utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
   bs::UtxoReservation::addAdapter(utxoAdapter_);

   connect(signingContainer_.get(), &SignContainer::QWalletInfo, this, &ReqCCSettlementContainer::onWalletInfo);

   const auto &signingWallet = transactionData_->getSigningWallet();
   if (signingWallet) {
      const auto &rootWallet = walletsMgr_->getHDRootForLeaf(signingWallet->walletId());
      walletInfo_ = bs::hd::WalletInfo(walletsMgr_, rootWallet);
      infoReqId_ = signingContainer_->GetInfo(walletInfo_.rootId().toStdString());
   }
   else {
      throw std::runtime_error("missing signing wallet");
   }

   lotSize_ = assetMgr_->getCCLotSize(product());

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

   if (side() == bs::network::Side::Sell) {
      dialogData.setValue("Title", tr("Settlement Delivery"));
   }
   else {
      dialogData.setValue("Title", tr("Settlement Payment"));
   }

   // rfq details
   dialogData.setValue("Price", UiUtils::displayPriceCC(price()));

   dialogData.setValue("Quantity", tr("%1 %2")
                 .arg(UiUtils::displayCCAmount(quantity()))
                 .arg(QString::fromStdString(product())));
   dialogData.setValue("TotalValue", UiUtils::displayAmount(quantity() * price()));

   // settlement details
   dialogData.setValue("Payment", tr("Verifying"));
   dialogData.setValue("GenesisAddress", tr("Verifying"));

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
      emit genAddrVerified(false, tr("GA is null"));
   }
   else if (side() == bs::network::Side::Buy) {
      emit info(tr("Waiting for genesis address verification to complete..."));

      const auto &cbHasInput = [this](bool has) {
         userKeyOk_ = has;
         emit genAddrVerified(has, has ? QString{} : tr("GA check failed"));
      };
      signer_.hasInputAddress(genAddress_, cbHasInput, lotSize_);
   }
   else {
      userKeyOk_ = true;
      emit genAddrVerified(true, QString{});
   }

   if (!createCCUnsignedTXdata()) {
      userKeyOk_ = false;
      emit error(tr("Failed to create unsigned CC transaction"));
   }

   startSigning();
}

void ReqCCSettlementContainer::deactivate()
{
   stopTimer();
}

bool ReqCCSettlementContainer::createCCUnsignedTXdata()
{
   const auto wallet = transactionData_->getSigningWallet();
   if (!wallet) {
      logger_->error("[CCSettlementTransactionWidget::createCCUnsignedTXdata] failed to get signing wallet");
      return false;
   }

   if (side() == bs::network::Side::Sell) {
      const uint64_t spendVal = quantity() * assetMgr_->getCCLotSize(product());
      logger_->debug("[CCSettlementTransactionWidget::createCCUnsignedTXdata] sell amount={}, spend value = {}", quantity(), spendVal);
      ccTxData_.walletId = wallet->walletId();
      ccTxData_.prevStates = { dealerTx_ };
      const auto recipient = bs::Address(dealerAddress_).getRecipient(spendVal);
      if (recipient) {
         ccTxData_.recipients.push_back(recipient);
      }
      else {
         logger_->error("[CCSettlementTransactionWidget::createCCUnsignedTXdata] failed to create recipient from {} and value {}"
            , dealerAddress_, spendVal);
         return false;
      }
      ccTxData_.populateUTXOs = true;
      ccTxData_.inputs = utxoAdapter_->get(id());
      logger_->debug("[CCSettlementTransactionWidget::createCCUnsignedTXdata] {} CC inputs reserved ({} recipients)"
         , ccTxData_.inputs.size(), ccTxData_.recipients.size());
      emit sendOrder();
   }
   else {
      const auto &cbFee = [this](float feePerByte) {
         const uint64_t spendVal = amount() * BTCNumericTypes::BalanceDivider;
         const auto &cbTxOutList = [this, feePerByte, spendVal](std::vector<UTXO> utxos) {
            try {
               const auto recipient = bs::Address(dealerAddress_).getRecipient(spendVal);
               if (!recipient) {
                  logger_->error("[CCSettlementTransactionWidget::createCCUnsignedTXdata] invalid recipient: {}", dealerAddress_);
                  return;
               }
               ccTxData_ = transactionData_->createPartialTXRequest(spendVal, feePerByte, { recipient }
               , dealerTx_, utxos);
               logger_->debug("{} inputs in ccTxData", ccTxData_.inputs.size());
               utxoAdapter_->reserve(ccTxData_.walletId, id(), ccTxData_.inputs);
               QMetaObject::invokeMethod(this, [this] { emit sendOrder(); });
            }
            catch (const std::exception &e) {
               logger_->error("[CCSettlementTransactionWidget::createCCUnsignedTXdata] Failed to create partial CC TX to {}: {}"
                  , dealerAddress_, e.what());
               QMetaObject::invokeMethod(this, [this] { emit error(tr("Failed to create CC TX half")); });
            }
         };
         if (!transactionData_->getWallet()->getSpendableTxOutList(cbTxOutList, spendVal)) {
            logger_->error("[CCSettlementTransactionWidget::createCCUnsignedTXdata] getSpendableTxOutList failed");
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

   const auto &cbTx = [this](bs::error::ErrorCode result, const BinaryData &signedTX) {
      if (result == bs::error::ErrorCode::NoError) {
         ccTxSigned_ = signedTX.toHexStr();
         emit settlementAccepted();
      }
      else if (result == bs::error::ErrorCode::TxCanceled) {
         emit settlementCancelled();
      }
      else {
         logger_->warn("[CCSettlementTransactionWidget::onTXSigned] CC TX sign failure: {}", bs::error::ErrorCodeToString(result).toStdString());
         emit error(tr("own TX half signing failed\n: %1").arg(bs::error::ErrorCodeToString(result)));
      }
   };

   ccSignId_ = signingContainer_->signSettlementPartialTXRequest(ccTxData_, toPasswordDialogData(), cbTx);
   logger_->debug("[CCSettlementTransactionWidget::createCCSignedTXdata] {} recipients", ccTxData_.recipients.size());
   return (ccSignId_ > 0);
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

bool ReqCCSettlementContainer::isAcceptable() const
{
   return userKeyOk_;
}

bool ReqCCSettlementContainer::cancel()
{
   deactivate();
   utxoAdapter_->unreserve(id());
   emit settlementCancelled();
   return true;
}

std::string ReqCCSettlementContainer::txData() const
{
   const auto &data = ccTxData_.serializeState().toHexStr();
   logger_->debug("[ReqCCSettlementContainer::txData] {}", data);
   return data;
}
