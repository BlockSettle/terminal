/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ReqCCSettlementContainer.h"
#include <spdlog/spdlog.h>
#include "AssetManager.h"
#include "CheckRecipSigner.h"
#include "SignContainer.h"
#include "TradesUtils.h"
#include "TransactionData.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "BSErrorCodeStrings.h"
#include "UiUtils.h"
#include "XBTAmount.h"
#include "UtxoReservationManager.h"

using namespace bs::sync;

ReqCCSettlementContainer::ReqCCSettlementContainer(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<AssetManager> &assetMgr
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const bs::network::RFQ &rfq
   , const bs::network::Quote &quote
   , const std::shared_ptr<bs::sync::hd::Wallet> &xbtWallet
   , const std::map<UTXO, std::string> &manualXbtInputs
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
   , bs::hd::Purpose walletPurpose
   , bs::UtxoReservationToken utxoRes
   , bool expandTxDialogInfo)
   : bs::SettlementContainer(std::move(utxoRes), walletPurpose, expandTxDialogInfo)
   , logger_(logger)
   , signingContainer_(container)
   , xbtWallet_(xbtWallet)
   , assetMgr_(assetMgr)
   , walletsMgr_(walletsMgr)
   , rfq_(rfq)
   , quote_(quote)
   , genAddress_(assetMgr_->getCCGenesisAddr(product()))
   , dealerAddress_(quote_.dealerAuthPublicKey)
   , signer_(armory)
   , lotSize_(assetMgr_->getCCLotSize(product()))
   , manualXbtInputs_(manualXbtInputs)
   , utxoReservationManager_(utxoReservationManager)
   , armory_(armory)
{
   if (!xbtWallet_) {
      throw std::logic_error("invalid hd wallet");
   }

   auto xbtGroup = xbtWallet_->getGroup(xbtWallet_->getXBTGroupType());
   if (!xbtGroup) {
      throw std::invalid_argument(fmt::format("can't find XBT group in {}", xbtWallet_->walletId()));
   }
   auto xbtLeaves = xbtGroup->getLeaves();
   xbtLeaves_.insert(xbtLeaves_.end(), xbtLeaves.begin(), xbtLeaves.end());
   if (xbtLeaves_.empty()) {
      throw std::invalid_argument(fmt::format("empty XBT group in {}", xbtWallet_->walletId()));
   }

   if (lotSize_ == 0) {
      throw std::runtime_error("invalid lot size");
   }

   ccWallet_ = walletsMgr->getCCWallet(rfq.product);
   if (!ccWallet_) {
      throw std::logic_error("can't find CC wallet");
   }

   connect(signingContainer_.get(), &SignContainer::QWalletInfo, this, &ReqCCSettlementContainer::onWalletInfo);
   connect(this, &ReqCCSettlementContainer::genAddressVerified, this
      , &ReqCCSettlementContainer::onGenAddressVerified, Qt::QueuedConnection);

   const auto &rootWallet = (rfq.side == bs::network::Side::Sell) ? walletsMgr_->getHDRootForLeaf(ccWallet_->walletId()) : xbtWallet_;
   if (!rootWallet) {
      throw std::runtime_error("missing signing wallet");
   }
   walletInfo_ = bs::hd::WalletInfo(walletsMgr_, rootWallet);
   infoReqId_ = signingContainer_->GetInfo(walletInfo_.rootId().toStdString());

   if (!dealerTx_.ParseFromString(BinaryData::CreateFromHex(quote_.dealerTransaction).toBinStr())) {
      throw std::invalid_argument("invalid dealer's transaction");
   }
}

ReqCCSettlementContainer::~ReqCCSettlementContainer() = default;

bs::sync::PasswordDialogData ReqCCSettlementContainer::toPasswordDialogData(QDateTime timestamp) const
{
   bs::sync::PasswordDialogData dialogData = SettlementContainer::toPasswordDialogData(timestamp);
   dialogData.setValue(PasswordDialogData::Market, "CC");
   dialogData.setValue(PasswordDialogData::AutoSignCategory, static_cast<int>(bs::signer::AutoSignCategory::SettlementRequestor));
   dialogData.setValue(PasswordDialogData::LotSize, static_cast<int>(lotSize_));

   if (side() == bs::network::Side::Sell) {
      dialogData.setValue(PasswordDialogData::Title, tr("Settlement Delivery"));
   }
   else {
      dialogData.setValue(PasswordDialogData::Title, tr("Settlement Payment"));
   }

   // rfq details
   dialogData.setValue(PasswordDialogData::Price, UiUtils::displayPriceCC(price()));
   dialogData.setValue(PasswordDialogData::Quantity, quantity());

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
      double balance = 0;
      for (const auto &leaf : xbtWallet_->getGroup(xbtWallet_->getXBTGroupType())->getLeaves()) {
         balance += assetMgr_->getBalance(bs::network::XbtCurrency, leaf);
      }
      if (amount() > balance) {
         emit paymentVerified(false, tr("Insufficient XBT balance in signing wallet"));
         return;
      }
   }

   startTimer(kWaitTimeoutInSec);

   userKeyOk_ = false;
   bool foundRecipAddr = false;
   bool amountValid = false;
   try {
      signer_.deserializeState(dealerTx_);
      foundRecipAddr = signer_.findRecipAddress(bs::Address::fromAddressString(rfq_.receiptAddress)
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
      emit error(id(), bs::error::ErrorCode::InternalError
         , tr("Failed to verify dealer's TX: %1").arg(QLatin1String(e.what())));
   }

   emit paymentVerified(foundRecipAddr && amountValid, QString{});

   if (genAddress_.empty()) {
      emit genAddressVerified(false, tr("GA is null"));
   }
   else if (side() == bs::network::Side::Buy) {
      //Waiting for genesis address verification to complete...

      const auto &cbHasInput = [this, handle = validityFlag_.handle()](bool has) {
         if (!handle.isValid()) {
            return;
         }
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
      emit error(id(), bs::error::ErrorCode::InternalError
         , tr("Failed to create unsigned CC transaction"));
   }
}

void ReqCCSettlementContainer::deactivate()
{
   stopTimer();
}

// KLUDGE currently this code not just making unsigned TX, but also initiate signing
bool ReqCCSettlementContainer::createCCUnsignedTXdata()
{
   if (side() == bs::network::Side::Sell) {
      const uint64_t spendVal = quantity() * assetMgr_->getCCLotSize(product());
      logger_->debug("[{}] sell amount={}, spend value = {}", __func__, quantity(), spendVal);
      ccTxData_.walletIds = { ccWallet_->walletId() };
      ccTxData_.armorySigner_.deserializeState(dealerTx_);
      const auto recipient = bs::Address::fromAddressString(dealerAddress_).getRecipient(bs::XBTAmount{ spendVal });
      if (recipient) {
         ccTxData_.armorySigner_.addRecipient(recipient, RECIP_GROUP_SPEND_1);
      }
      else {
         logger_->error("[{}] failed to create recipient from {} and value {}"
            , __func__, dealerAddress_, spendVal);
         return false;
      }

      logger_->debug("[{}] {} CC inputs reserved ({} recipients)"
         , __func__, ccTxData_.armorySigner_.getTxInCount(), 
                     ccTxData_.armorySigner_.getTxOutCount());

      // KLUDGE - in current implementation, we should sign first to have sell/buy process aligned
      AcceptQuote();
   }
   else {
      const auto &cbFee = [this](float feePerByteArmory) {
         auto feePerByte = std::max(feePerByteArmory, utxoReservationManager_->feeRatePb());
         const uint64_t spendVal = bs::XBTAmount(amount()).GetValue();
         auto inputsCb = [this, feePerByte, spendVal](const std::map<UTXO, std::string> &xbtInputs, bool useAllInputs = false) {
            auto changeAddrCb = [this, feePerByte, xbtInputs, spendVal, useAllInputs](const bs::Address &changeAddr) {
               try {

                  const auto recipient = bs::Address::fromAddressString(dealerAddress_).getRecipient(bs::XBTAmount{ spendVal });
                  if (!recipient) {
                     logger_->error("[{}] invalid recipient: {}", __func__, dealerAddress_);
                     return;
                  }
                  std::map<unsigned, std::vector<std::shared_ptr<ArmorySigner::ScriptRecipient>>> recipientMap;
                  std::vector<std::shared_ptr<ArmorySigner::ScriptRecipient>> recVec({recipient});
                  recipientMap.emplace(RECIP_GROUP_SPEND_2, std::move(recVec));

                  ccTxData_ = bs::sync::WalletsManager::createPartialTXRequest(spendVal
                     , xbtInputs, changeAddr, feePerByte, armory_->topBlock()
                     , recipientMap, RECIP_GROUP_CHANG_2
                     , dealerTx_, useAllInputs, UINT32_MAX, logger_);

                  logger_->debug("{} inputs in ccTxData", ccTxData_.armorySigner_.getTxInCount());
                  // Must release old reservation first (we reserve excessive XBT inputs in advance for CC buy requests)!

                  auto resolveCB = [this](
                     bs::error::ErrorCode result, const Codec_SignerState::SignerState &state)
                  {
                     utxoRes_.release();
                     if (result != bs::error::ErrorCode::NoError) {
                        std::stringstream ss;
                        ss << "failed to resolve CC half reply with error code: " << (int)result;
                        throw std::runtime_error(ss.str());
                     }

                     ccTxData_.armorySigner_.deserializeState(state);
                     utxoRes_ = utxoReservationManager_->makeNewReservation(ccTxData_.getInputs(nullptr), id());
                     AcceptQuote();
                  };
                  signingContainer_->resolvePublicSpenders(ccTxData_, resolveCB);
               }
               catch (const std::exception &e) {
                  SPDLOG_LOGGER_ERROR(logger_, "Failed to create partial CC TX "
                     "to {}: {}", dealerAddress_, e.what());
                  emit error(id(), bs::error::ErrorCode::InternalError
                     , tr("Failed to create CC TX half"));
               }
            };
            xbtLeaves_.front()->getNewChangeAddress(changeAddrCb);
         };
         if (manualXbtInputs_.empty()) {
            std::vector<UTXO> utxos;
            if (!xbtWallet_->canMixLeaves()) {
               utxos = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet_->walletId(), walletPurpose_);
            }
            else {
               utxos = utxoReservationManager_->getAvailableXbtUTXOs(xbtWallet_->walletId());
            }

            auto fixedUtxo = utxoReservationManager_->convertUtxoToPartialFixedInput(xbtWallet_->walletId(), utxos);
            inputsCb(fixedUtxo.inputs);
         } else {
            inputsCb(manualXbtInputs_, true);
         }
      };
      walletsMgr_->estimatedFeePerByte(0, cbFee, this);
   }

   return true;
}

void ReqCCSettlementContainer::AcceptQuote()
{
   if (side() == bs::network::Side::Sell) {
      if (!ccTxData_.isValid()) {
         logger_->error("[CCSettlementTransactionWidget::AcceptQuote] CC TX half wasn't created properly");
         emit error(id(), bs::error::ErrorCode::InternalError
            , tr("Failed to create TX half"));
         return;
      }
   }
   signingContainer_->resolvePublicSpenders(ccTxData_, [this]
      (bs::error::ErrorCode result, const Codec_SignerState::SignerState &state)
   {
      if (result == bs::error::ErrorCode::NoError) {
         ccTxResolvedData_ = state;
         emit sendOrder();
      }
      else {
         emit error(id(), result, bs::error::ErrorCodeToString(result));
      }
   });
}

bool ReqCCSettlementContainer::startSigning(QDateTime timestamp)
{
   const auto &cbTx = [this, handle = validityFlag_.handle(), logger=logger_](bs::error::ErrorCode result, const BinaryData &signedTX) {
      if (!handle.isValid()) {
         logger->warn("[ReqCCSettlementContainer::onTXSigned] failed to sign TX half, already destroyed");
         return;
      }

      ccSignId_ = 0;

      if (result == bs::error::ErrorCode::NoError) {
         ccTxSigned_ = signedTX.toHexStr();

         // notify RFQ dialog that signed half could be saved
         emit txSigned();

         // FIXME: disabled as it does not work correctly (signedTX txid is different from combined txid)
#if 0
         for (const auto &xbtLeaf : xbtLeaves_) {
            xbtLeaf->setTransactionComment(signedTX, txComment());
         }
         ccWallet_->setTransactionComment(signedTX, txComment());
#endif
      }
      else if (result == bs::error::ErrorCode::TxCancelled) {
         SettlementContainer::releaseUtxoRes();
         emit cancelTrade(clOrdId_);
      }
      else {
         logger->error("[CCSettlementTransactionWidget::onTXSigned] CC TX sign failure: {}", bs::error::ErrorCodeToString(result).toStdString());
         emit error(id(), result, tr("Own TX half signing failed: %1")
            .arg(bs::error::ErrorCodeToString(result)));
      }

      // Call completed to remove from RfqStorage and cleanup memory
      emit completed(id());
   };

   ccSignId_ = signingContainer_->signSettlementPartialTXRequest(ccTxData_, toPasswordDialogData(timestamp), cbTx);
   logger_->debug(
      "[CCSettlementTransactionWidget::createCCSignedTXdata] {} recipients", 
      ccTxData_.armorySigner_.getTxInCount());
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
   pd.setValue(PasswordDialogData::SettlementId, id());
   pd.setValue(PasswordDialogData::DeliveryUTXOVerified, addressVerified);
   pd.setValue(PasswordDialogData::SigningAllowed, addressVerified);
   signingContainer_->updateDialogData(pd);
}

bool ReqCCSettlementContainer::cancel()
{
   deactivate();
   if (ccSignId_ != 0) {
      signingContainer_->CancelSignTx(BinaryData::fromString(id()));
   }

   SettlementContainer::releaseUtxoRes();
   emit settlementCancelled();

   return true;
}

std::string ReqCCSettlementContainer::txData() const
{
   if (!ccTxResolvedData_.IsInitialized()) {
      logger_->error("[ReqCCSettlementContainer::txData] no resolved data");
      return {};
   }
   return BinaryData::fromString(ccTxResolvedData_.SerializeAsString()).toHexStr();
}

void ReqCCSettlementContainer::setClOrdId(const std::string& clientOrderId)
{
   clOrdId_ = clientOrderId;
}
