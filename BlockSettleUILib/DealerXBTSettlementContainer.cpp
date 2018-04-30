#include "DealerXBTSettlementContainer.h"
#include <spdlog/spdlog.h>
#include "SignContainer.h"
#include "QuoteProvider.h"
#include "TransactionData.h"
#include "WalletsManager.h"

Q_DECLARE_METATYPE(AddressVerificationState)

DealerXBTSettlementContainer::DealerXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &logger, const bs::network::Order &order
   , const std::shared_ptr<WalletsManager> &walletsMgr, const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<TransactionData> &txData, const std::unordered_set<std::string> &bsAddresses
   , const std::shared_ptr<SignContainer> &container, bool autoSign)
   : bs::SettlementContainer(), order_(order)
   , weSell_((order.side == bs::network::Side::Buy) ^ (order.product == bs::network::XbtCurrency))
   , amount_((order.product != bs::network::XbtCurrency) ? order.quantity / order.price : order.quantity)
   , autoSign_(autoSign), logger_(logger), transactionData_(txData), signingContainer_(container)
{
   qRegisterMetaType<AddressVerificationState>();

   if (weSell_ && !transactionData_->GetRecipientsCount()) {
      throw std::runtime_error("no recipient[s]");
   }
   if (transactionData_->GetWallet() == nullptr) {
      throw std::runtime_error("no wallet");
   }

   settlWallet_ = walletsMgr->GetSettlementWallet();
   if (!settlWallet_) {
      throw std::runtime_error("missing settlement wallet");
   }

   comment_ = std::string(bs::network::Side::toString(order.side)) + " " + order.security + " @ " + std::to_string(order.price);
   settlAddr_ = settlWallet_->getExistingAddress(BinaryData::CreateFromHex(order.settlementId));
   if (settlAddr_ == nullptr) {
      auto qn = quoteProvider->getSubmittedXBTQuoteNotification(order.settlementId);
      if (qn.authKey.empty() || qn.reqAuthKey.empty() || qn.settlementId.empty()) {
         throw std::invalid_argument("failed to get submitted QN for " + order.quoteId);
      }
      authKey_ = BinaryData::CreateFromHex(qn.authKey);
      reqAuthKey_ = BinaryData::CreateFromHex(qn.reqAuthKey);
      settlIdStr_ = qn.settlementId;
      const auto buyAuthKey = weSell_ ? reqAuthKey_ : authKey_;
      const auto sellAuthKey = weSell_ ? authKey_ : reqAuthKey_;

      settlAddr_ = walletsMgr->GetSettlementWallet()->newAddress(
         BinaryData::CreateFromHex(settlIdStr_), buyAuthKey, sellAuthKey, comment_);
   }
   else {
      if (weSell_) {
         authKey_ = settlAddr_->getAsset()->sellAuthPubKey();
         reqAuthKey_ = settlAddr_->getAsset()->buyAuthPubKey();
      }
      else {
         authKey_ = settlAddr_->getAsset()->buyAuthPubKey();
         reqAuthKey_ = settlAddr_->getAsset()->sellAuthPubKey();
      }
      settlIdStr_ = settlAddr_->getAsset()->settlementId().toHexStr();
   }

   settlMonitor_ = settlWallet_->createMonitor(settlAddr_, logger);
   if (settlMonitor_ == nullptr) {
      throw std::runtime_error("failed to create Settlement monitor");
   }

   addrVerificator_ = std::make_shared<AddressVerificator>(logger, settlIdStr_
      , [this, logger](const std::shared_ptr<AuthAddress>& address, AddressVerificationState state)
   {
      logger->info("Counterparty's address verification {} for {}"
         , to_string(state), address->GetChainedAddress().display<std::string>());
      cptyAddressState_ = state;
      emit cptyAddressStateChanged(state);
      if (state == AddressVerificationState::Verified) {
         onCptyVerified();
      }
   });
   addrVerificator_->SetBSAddressList(bsAddresses);

   connect(settlMonitor_.get(), &bs::SettlementMonitor::payInDetected, this, &DealerXBTSettlementContainer::onPayInDetected);
   connect(settlMonitor_.get(), &bs::SettlementMonitor::payOutDetected, this, &DealerXBTSettlementContainer::onPayOutDetected);

   connect(signingContainer_.get(), &SignContainer::TXSigned, this, &DealerXBTSettlementContainer::onTXSigned);
}

bool DealerXBTSettlementContainer::accept(const std::string& password)
{
   if (weSell_) {
      try {
         const auto txReq = transactionData_->GetSignTXRequest();
         payinSignId_ = signingContainer_->SignTXRequest(txReq, autoSign_
            , SignContainer::TXSignMode::Full, password);
      }
      catch (const std::exception &e) {
         logger_->error("[DealerXBTSettlementContainer::onAccepted] Failed to sign pay-in: {}", e.what());
         emit error(tr("Failed to sign pay-in"));
         emit failed();
         return false;
      }
   }
   else {
      const auto &receivingAddress = transactionData_->GetFallbackRecvAddress();
      signingContainer_->SyncAddresses({ { transactionData_->GetWallet(), receivingAddress } });
      try {
         const auto txReq = settlWallet_->CreatePayoutTXRequest(settlWallet_->GetInputFor(settlAddr_)
            , receivingAddress, transactionData_->FeePerByte());
         const auto authAddr = bs::Address::fromPubKey(authKey_, AddressEntryType_P2WPKH);
         payoutSignId_ = signingContainer_->SignPayoutTXRequest(txReq, authAddr, settlAddr_
            , autoSign_, password);
      }
      catch (const std::exception &e) {
         logger_->error("[DealerSettlDialog::onAccepted] Failed to sign pay-out: {}", e.what());
         emit error(tr("Failed to sign pay-out"));
         emit failed();
         return false;
      }
   }
   return true;
}

bool DealerXBTSettlementContainer::cancel()
{
   return true;
}

bool DealerXBTSettlementContainer::isAcceptable() const
{
   if (cptyAddressState_ != AddressVerificationState::Verified) {
      return false;
   }
   return weSell_ ? !payInSent_ : payInDetected_;
}

void DealerXBTSettlementContainer::activate()
{
   startTimer(30);

   signingContainer_->SyncAddresses(transactionData_->createAddresses());

   const auto reqAuthAddrSW = bs::Address::fromPubKey(reqAuthKey_, AddressEntryType_P2WPKH);
   addrVerificator_->StartAddressVerification(std::make_shared<AuthAddress>(reqAuthAddrSW));
   addrVerificator_->RegisterAddresses();

   settlMonitor_->start();
}

void DealerXBTSettlementContainer::deactivate()
{
   stopTimer();
   if (settlMonitor_) {
      settlMonitor_->stop();
      settlMonitor_ = nullptr;
   }
}

void DealerXBTSettlementContainer::onPayInDetected(int confirmationsNumber, const BinaryData &txHash)
{
   if (payInDetected_) {
      return;
   }
   logger_->debug("[XbtSettlementContainer] Pay-in detected: {}", txHash.toHexStr(true));

   if (!weSell_) {
      const auto tx = PyBlockDataManager::instance()->getTxByHash(txHash);
      bool foundAddr = false, amountValid = false;
      if (tx.isInitialized()) {
         for (int i = 0; i < tx.getNumTxOut(); i++) {
            auto txOut = tx.getTxOutCopy(i);
            const auto addr = bs::Address::fromTxOutScript(txOut.getScript());
            if (settlAddr_->getPrefixedHash() == addr.prefixed()) {
               foundAddr = true;
               if (std::abs(txOut.getValue() - amount_ * BTCNumericTypes::BalanceDivider) < 3) {
                  amountValid = true;
               }
               break;
            }
         }
      }
      if (!foundAddr || !amountValid) {
         emit error(tr("Invalid pay-in transaction from requester"));
         return;
      }

      try {
         fee_ = settlWallet_->GetEstimatedFeeFor(settlWallet_->GetInputFor(settlAddr_), transactionData_->GetFallbackRecvAddress()
            , transactionData_->FeePerByte());
      }
      catch (const std::exception &e) {
         throw std::runtime_error(std::string("Settlement input retrieval failed: ") + e.what());
      }
   }

   payInDetected_ = true;
   emit payInDetected(confirmationsNumber, txHash);
   startTimer(30);
   onCptyVerified();
}

void DealerXBTSettlementContainer::onPayOutDetected(int confirmationsNumber, bs::PayoutSigner::Type signedBy)
{
   logger_->debug("[XbtSettlementContainer] Pay-out detected! Signed by {}"
      , bs::PayoutSigner::toString(signedBy));
   bool settlementFailed = false;

   if (signedBy == bs::PayoutSigner::SignatureUndefined) {
      logger_->error("[DealerXBTSettlementContainer::payOutDetected] Signature undefined");
      emit error(tr("Pay-out signed by undefined key"));
      settlementFailed = true;
   }
   else {
      if (signedBy == bs::PayoutSigner::SignedBySeller) {
         if (weSell_) {
            logger_->error("[DealerXBTSettlementContainer::payOutDetected] pay-out signed by dealer on dealer sell");
            emit error(tr("Pay-out signed by dealer"));
         }
         else {
            logger_->error("[DealerXBTSettlementContainer::payOutDetected] pay-out signed by requestor on dealer buy");
            emit error(tr("Pay-out signed by requestor"));
            emit completed();
            return;
         }
         settlementFailed = true;
      }
   }

   if (settlementFailed) {
      stopTimer();
      emit failed();
   }
   else {
      emit completed();
   }
}

void DealerXBTSettlementContainer::onTXSigned(unsigned int id, BinaryData signedTX, std::string errMsg)
{
   if (payoutSignId_ && (payoutSignId_ == id)) {
      payoutSignId_ = 0;
      if (!errMsg.empty()) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] Failed to sign pay-out: {}", errMsg);
         emit error(tr("Failed to sign pay-out"));
         emit failed();
         return;
      }
      if (!PyBlockDataManager::instance()->broadcastZC(signedTX)) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] Failed to broadcast pay-out");
         emit error(tr("Failed to broadcast pay-out transaction"));
         emit failed();
         return;
      }
      transactionData_->GetWallet()->SetTransactionComment(signedTX, comment_);
      settlWallet_->SetTransactionComment(signedTX, comment_);
      transactionData_->GetWallet()->SetAddressComment(transactionData_->GetFallbackRecvAddress()
         , bs::wallet::Comment::toString(bs::wallet::Comment::SettlementPayOut));

      logger_->debug("[DealerXBTSettlementContainer::onTXSigned] Payout sent");
      emit info(tr("Pay-out broadcasted. Waiting to appear on chain"));
   }
   else if (payinSignId_ && (payinSignId_ == id)) {
      payinSignId_ = 0;
      if (!errMsg.empty()) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] Failed to sign pay-in: {}", errMsg);
         emit error(tr("Failed to sign pay-in"));
         emit failed();
         return;
      }
      if (!PyBlockDataManager::instance()->broadcastZC(signedTX)) {
         logger_->error("[DealerXBTSettlementContainer::onTXSigned] Failed to broadcast pay-in");
         emit error(tr("Failed to broadcast transaction"));
         emit failed();
         return;
      }
      transactionData_->GetWallet()->SetTransactionComment(signedTX, comment_);
      settlWallet_->SetTransactionComment(signedTX, comment_);

      logger_->debug("[DealerXBTSettlementContainer::onAccepted] Payin sent");
      payInSent_ = true;
   }
}

void DealerXBTSettlementContainer::onCptyVerified()
{
   if (payInDetected_) {
      if (weSell_) {
         emit info(tr("Own pay-in detected - now sending requester's pay-out"));
         sendBuyReqPayout();
      }
      else {
         emit info(tr("Accept blockchain transaction"));
         emit readyToAccept();
      }
   }
   else {
      if (weSell_) {
         emit info(tr("Accept to send own pay-in transaction"));
         emit readyToAccept();
      }
      else {
         emit info(tr("Waiting for counterparty's transaction in blockchain..."));
      }
   }
}

void DealerXBTSettlementContainer::sendBuyReqPayout()
{
   const auto payoutTx = BinaryData::CreateFromHex(order_.reqTransaction);
   if (payoutTx.isNull()) {
      emit error(tr("Failed to get requestor pay-out transaction"));
      logger_->error("[DealerXBTSettlementContainer::sendBuyReqPayout] Failed to get pay-out transaction");
      return;
   }

   logger_->debug("[DealerXBTSettlementContainer::sendBuyReqPayout] sending tx with hash {}"
      , BtcUtils::hash256(payoutTx).toHexStr());
   if (!PyBlockDataManager::instance()->broadcastZC(payoutTx)) {
      emit error(tr("Failed to broadcast pay-out transaction"));
      logger_->error("[DealerXBTSettlementContainer::sendBuyReqPayout] Failed to broadcast pay-out transaction");
      return;
   }

   emit info(tr("Waiting for pay-out on chain"));
}

std::string DealerXBTSettlementContainer::walletName() const
{
   return transactionData_->GetWallet()->GetWalletName();
}

bs::Address DealerXBTSettlementContainer::receiveAddress() const
{
   if (weSell_) {
      return transactionData_->GetRecipientAddress(0);
   }
   return transactionData_->GetFallbackRecvAddress();
}
