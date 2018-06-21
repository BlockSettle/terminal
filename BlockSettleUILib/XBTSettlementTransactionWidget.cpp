#include "XBTSettlementTransactionWidget.h"

#include "ui_XBTSettlementTransactionWidget.h"

#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "CurrencyPair.h"
#include "HDWallet.h"
#include "QuoteProvider.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "WalletsManager.h"

#include <QtConcurrent/QtConcurrentRun>

#include <spdlog/logger.h>

const unsigned int WaitTimeoutInSec = 30;

XBTSettlementTransactionWidget::XBTSettlementTransactionWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::XBTSettlementTransactionWidget())
   , timer_(this)
   , sValid(tr("<span style=\"color: #22C064;\">Verified</span>"))
   , sInvalid(tr("<span style=\"color: #CF292E;\">Invalid</span>"))
   , sFailed(tr("<span style=\"color: #CF292E;\">Failed</span>"))
   , waitForPayout_(false), waitForPayin_(false)
   , sellFromPrimary_(false)
{
   ui_->setupUi(this);

   setupTimer();

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &XBTSettlementTransactionWidget::onCancel);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &XBTSettlementTransactionWidget::onAccept);
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &XBTSettlementTransactionWidget::updateAcceptButton);
   connect(ui_->lineEditAuthPassword, &QLineEdit::textChanged, this, &XBTSettlementTransactionWidget::updateAcceptButton);

   ui_->pushButtonAccept->setEnabled(false);

   connect(this, &XBTSettlementTransactionWidget::DealerVerificationStateChanged
      , this, &XBTSettlementTransactionWidget::onDealerVerificationStateChanged
      , Qt::QueuedConnection);
}

XBTSettlementTransactionWidget::~XBTSettlementTransactionWidget()
{
   bs::UtxoReservation::delAdapter(utxoAdapter_);
}

void XBTSettlementTransactionWidget::setupTimer()
{
   ui_->progressBar->setMaximum(WaitTimeoutInSec * 1000);
   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &XBTSettlementTransactionWidget::ticker);
}

void XBTSettlementTransactionWidget::onCancel()
{
   cancel();
   emit settlementCancelled();
}

void XBTSettlementTransactionWidget::cancel()
{
   timer_.stop();
   if (clientSells_) {
      if (!payoutData_.isNull()) {
         payoutOnCancel();
      }
      utxoAdapter_->unreserve(reserveId_);
   }
}

void XBTSettlementTransactionWidget::reset(const std::shared_ptr<WalletsManager> &walletsManager)
{
   walletsManager_ = walletsManager;
   expireTime_ = QDateTime::currentDateTime().addSecs(WaitTimeoutInSec);
   ticker();
   timer_.start();
   ui_->pushButtonCancel->setEnabled(true);

   ui_->lineEditAuthPassword->setEnabled(true);
   ui_->lineEditPassword->setEnabled(true);
}

void XBTSettlementTransactionWidget::ticker()
{
   const auto timeDiff = QDateTime::currentDateTime().msecsTo(expireTime_);
   if (timeDiff < 0) {
      timer_.stop();
      if (!clientSells_ && (waitForPayin_ || waitForPayout_)) {
         ui_->labelHintPassword->setText(tr("Dealer didn't push transactions to blockchain within a given time interval"));
         emit settlementCancelled();
      } else {
         if (clientSells_ && waitForPayout_) {
            waitForPayout_ = false;
         }
         ui_->pushButtonCancel->setEnabled(false);
         emit settlementCancelled();
         utxoAdapter_->unreserve(reserveId_);
      }
   } else {
      ui_->progressBar->setValue(timeDiff);
      ui_->progressBar->setFormat(tr("%n second(s) remaining", "", timeDiff / 1000));
   }
}

void XBTSettlementTransactionWidget::populateDetails(const bs::network::RFQ& rfq, const bs::network::Quote& quote
   , const std::shared_ptr<TransactionData>& transactionData)
{
   transactionData_ = transactionData;
   rfq_ = rfq;
   quote_ = quote;

   product_ = rfq.product;
   QString qtyProd = UiUtils::XbtCurrency;

   CurrencyPair cp(quote.security);
   auto fxProd = QString::fromStdString(cp.ContraCurrency(bs::network::XbtCurrency));
   const bool isFxProd = (quote.product != bs::network::XbtCurrency);
   amount_ = isFxProd ? quote.quantity / quote.price : quote.quantity;

   price_ = quote.price;

   ui_->labelProductGroup->setText(tr(bs::network::Asset::toString(rfq.assetType)));
   ui_->labelSecurityID->setText(QString::fromStdString(rfq.security));
   ui_->labelProduct->setText(QString::fromStdString(rfq.product));
   ui_->labelSide->setText(tr(bs::network::Side::toString(rfq.side)));

   ui_->labelQuantity->setText(tr("%1 %2")
      .arg(UiUtils::displayAmountForProduct(amount_, qtyProd, bs::network::Asset::Type::SpotXBT))
      .arg(qtyProd));

   ui_->labelPrice->setText(UiUtils::displayPriceXBT(quote.price));

   ui_->labelTotalValue->setText(tr("%1 %2")
      .arg(UiUtils::displayAmountForProduct(amount_ * price_, fxProd, bs::network::Asset::Type::SpotXBT))
      .arg(fxProd));

   clientSells_ = !rfq.isXbtBuy();
   populateXBTDetails(quote);

   auto authWallet = walletsManager_->GetAuthWallet();
   auto rootAuthWallet = walletsManager_->GetHDRootForLeaf(authWallet->GetWalletId());

   if (clientSells_) {
      const auto rootAuthWalletId = rootAuthWallet->getWalletId();
      const auto rootSellingWalletId = walletsManager_->GetHDRootForLeaf(transactionData_->GetWallet()->GetWalletId())->getWalletId();
      sellFromPrimary_ = rootAuthWalletId == rootSellingWalletId;

      ui_->labelHintPassword->setText(tr("Enter password for \"%1\" wallet to sign Pay-In")
         .arg(QString::fromStdString(walletsManager_->GetHDRootForLeaf(
            transactionData_->GetWallet()->GetWalletId())->getName())));

      if (sellFromPrimary_) {
         ui_->labelHintAuthPassword->hide();
         ui_->horizontalWidgetAuthPassword->hide();
      } else {
         ui_->labelHintAuthPassword->setText(tr("Enter password for \"%1\" wallet to sign revoke Pay-Out")
            .arg(QString::fromStdString(rootAuthWallet->getName())));
      }
   } else {
      ui_->labelHintPassword->setText(tr("Enter password for \"%1\" wallet to sign Pay-Out")
         .arg(QString::fromStdString(rootAuthWallet->getName())));
      ui_->labelHintAuthPassword->hide();
      ui_->horizontalWidgetAuthPassword->hide();
   }

   updateAcceptButton();
}

void XBTSettlementTransactionWidget::onDealerVerificationStateChanged()
{
   QString text;
   switch (dealerVerifState_) {
   case AddressVerificationState::Verified:
      text = sValid;
      break;
   case AddressVerificationState::VerificationFailed:
      text = sFailed;
      break;
   default:
      text = sInvalid;
      break;
   }

   ui_->labelDealerAuthAddress->setText(text);
   updateAcceptButton();
}

void XBTSettlementTransactionWidget::populateXBTDetails(const bs::network::Quote& quote)
{
   addrVerificator_ = std::make_shared<AddressVerificator>(logger_, quote.settlementId
      , [this](const std::shared_ptr<AuthAddress>& address, AddressVerificationState state)
   {
      dealerVerifState_ = state;

      emit DealerVerificationStateChanged();
   });
   addrVerificator_->SetBSAddressList(authAddressManager_->GetBSAddresses());

   reserveId_ = quote.requestId;
   settlementId_ = BinaryData::CreateFromHex(quote.settlementId);
   userKey_ = BinaryData::CreateFromHex(quote.requestorAuthPublicKey);
   const auto dealerAuthKey = BinaryData::CreateFromHex(quote.dealerAuthPublicKey);
   const auto buyAuthKey = clientSells_ ? dealerAuthKey : userKey_;
   const auto sellAuthKey = clientSells_ ? userKey_ : dealerAuthKey;
   comment_ = std::string(bs::network::Side::toString(bs::network::Side::invert(quote.side))) + " "
      + quote.security + " @ " + std::to_string(quote.price);

   settlAddr_ = walletsManager_->GetSettlementWallet()->newAddress(settlementId_, buyAuthKey, sellAuthKey, comment_);

   if (settlAddr_ == nullptr) {
      logger_->error("[XBTSettlementTransactionWidget] failed to get settlement address");
      return;
   }

   recvAddr_ = transactionData_->GetFallbackRecvAddress();
   signingContainer_->SyncAddresses({ {transactionData_->GetWallet(), recvAddr_} });

   const auto recipient = transactionData_->RegisterNewRecipient();
   transactionData_->UpdateRecipientAmount(recipient, amount_, transactionData_->maxSpendAmount());
   transactionData_->UpdateRecipientAddress(recipient, settlAddr_);

   const auto dealerAddrSW = bs::Address::fromPubKey(dealerAuthKey, AddressEntryType_P2WPKH);
   ui_->labelDealerAuthAddress->setText(tr("Validating"));
   addrVerificator_->StartAddressVerification(std::make_shared<AuthAddress>(dealerAddrSW));
   addrVerificator_->RegisterAddresses();

   const auto list = authAddressManager_->GetVerifiedAddressList();
   const auto userAddress = bs::Address::fromPubKey(userKey_, AddressEntryType_P2WPKH);
   userKeyOk_ = (std::find(list.begin(), list.end(), userAddress) != list.end());

    ui_->labelUserAuthAddress->setText(userKeyOk_ ? sValid : sInvalid);

   if (clientSells_) {
      window()->setWindowTitle(tr("Settlement Pay-In Transaction"));

      // addDetailRow(tr("Sending wallet"), tr("<b>%1</b> (%2)").arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletName()))
      //    .arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletId())));
      // addDetailRow(tr("Number of inputs"), tr("<b>%L1</b>")
      //    .arg(QString::number(transactionData_->GetTransactionSummary().usedTransactions)));

      ui_->labelHintPassword->setText(tr("Enter Password and Accept to send Pay-In"));

      if (!transactionData_->IsTransactionValid()) {
         userKeyOk_ = false;
         ui_->labelHintPassword->setText(tr("Transaction data is invalid - sending of pay-in is prohibited"));
      }
   }
   else {
      window()->setWindowTitle(tr("Settlement Pay-Out Transaction"));

      // addDetailRow(tr("Receiving wallet"), tr("<b>%1</b> (%2)").arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletName()))
      //    .arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletId())));
      // addDetailRow(tr("Receiving address"), tr("<b>%1</b>").arg(recvAddr_.display()));

      ui_->labelHintPassword->setText(tr("Enter Password and Accept to send Pay-Out to dealer"));

      dealerTx_ = BinaryData::CreateFromHex(quote.dealerTransaction);
   }

   monitor_ = walletsManager_->GetSettlementWallet()->createMonitor(settlAddr_, logger_);
   if (monitor_ == nullptr) {
      logger_->error("[XBTSettlementTransactionWidget::populateXBTDetails] failed to create Settlement monitor");
      return;
   }
   connect(monitor_.get(), &bs::SettlementMonitor::payInDetected, this, &XBTSettlementTransactionWidget::onPayInZCDetected);
   connect(monitor_.get(), &bs::SettlementMonitor::payOutDetected, this, &XBTSettlementTransactionWidget::onPayoutZCDetected);
   monitor_->start();

   ui_->labelTransactionAmount->setText(UiUtils::displayQuantity(amount_, UiUtils::XbtCurrency));

   const auto fee = transactionData_->GetTransactionSummary().totalFee;
   ui_->labelFee->setText(UiUtils::displayQuantity(UiUtils::amountToBtc(fee), UiUtils::XbtCurrency));

   if (clientSells_) {
      ui_->labelTotalDescription->setText(tr("Total spent"));
      ui_->labelTotalAmount->setText(UiUtils::displayQuantity(amount_ + UiUtils::amountToBtc(fee), UiUtils::XbtCurrency));
   } else {
      ui_->labelTotalDescription->setText(tr("Total received"));
      ui_->labelTotalAmount->setText(UiUtils::displayQuantity(amount_ - UiUtils::amountToBtc(fee), UiUtils::XbtCurrency));
   }

   connect(PyBlockDataManager::instance().get(), &PyBlockDataManager::txBroadcastError, this, &XBTSettlementTransactionWidget::onZCError, Qt::QueuedConnection);
}

unsigned int XBTSettlementTransactionWidget::createPayoutTx(const BinaryData& payinHash, double qty, const bs::Address &recvAddr)
{
   try {
      const auto &wallet = walletsManager_->GetSettlementWallet();
      const auto txReq = wallet->CreatePayoutTXRequest(wallet->GetInputFromTX(settlAddr_, payinHash, qty), recvAddr
         , transactionData_->GetTransactionSummary().feePerByte);
      const auto authAddr = bs::Address::fromPubKey(userKey_, AddressEntryType_P2WPKH);
      logger_->debug("[XBTSettlementTransactionWidget] pay-out fee={}, payin hash={}", txReq.fee, payinHash.toHexStr(true));
      std::string password;

      if (clientSells_) {
         if (sellFromPrimary_) {
            password = ui_->lineEditPassword->text().toStdString();
         } else {
            password = ui_->lineEditAuthPassword->text().toStdString();
         }
      } else {
         password = ui_->lineEditPassword->text().toStdString();
      }

      return signingContainer_->SignPayoutTXRequest(txReq, authAddr, settlAddr_, false, password);
   }
   catch (const std::exception &e) {
      logger_->warn("[XBTSettlementTransactionWidget] failed to create pay-out transaction based on {}: {}"
         , payinHash.toHexStr(), e.what());
      ui_->labelHintPassword->setText(tr("Pay-out transaction creation failure"));
   }
   return 0;
}

void XBTSettlementTransactionWidget::onAccept()
{
   ui_->labelHintPassword->clear();
   ui_->pushButtonAccept->setEnabled(false);

   if (payinData_.isNull()) {
      acceptSpotXBT();
   }
   else {
      payoutSignId_ = createPayoutTx(Tx(payinData_).getThisHash(), amount_, recvAddr_);
   }
}

void XBTSettlementTransactionWidget::stop()
{
   timer_.stop();
   ui_->progressBar->setValue(0);
   ui_->lineEditPassword->setEnabled(false);
   ui_->labelHintAuthPassword->clear();
   ui_->lineEditAuthPassword->setEnabled(false);
}

void XBTSettlementTransactionWidget::retry()
{
   ui_->pushButtonAccept->setText(tr("Retry"));
   ui_->lineEditPassword->clear();
   ui_->lineEditAuthPassword->setEnabled(true);
   ui_->pushButtonAccept->setEnabled(true);
}

void XBTSettlementTransactionWidget::acceptSpotXBT()
{
   ui_->labelHintPassword->setText(tr("Waiting for transactions signing..."));
   if (clientSells_) {
      const auto hasChange = transactionData_->GetTransactionSummary().hasChange;
      const auto changeAddr = hasChange ? transactionData_->GetWallet()->GetNewChangeAddress() : bs::Address();
      const auto payinTxReq = transactionData_->CreateTXRequest(false, changeAddr);
      payinSignId_ = signingContainer_->SignTXRequest(payinTxReq, false, SignContainer::TXSignMode::Full
         , ui_->lineEditPassword->text().toStdString());
   } else {
      ui_->pushButtonCancel->setEnabled(false);
      try {    // create payout based on dealer TX
         if (dealerTx_.isNull()) {
            logger_->error("[XBTSettlementTransactionWidget::acceptSpotXBT] empty dealer payin hash");
         } else {
            payoutSignId_ = createPayoutTx(dealerTx_, amount_, recvAddr_);
         }
      }
      catch (const std::exception &e) {
         logger_->warn("[XBTSettlementTransactionWidget::acceptSpotXBT] Pay-Out failed: {}", e.what());
         ui_->labelHintPassword->setText(tr("Pay-Out to dealer failed: %1")
            .arg(QString::fromStdString(e.what())));
         return;
      }
   }
}

void XBTSettlementTransactionWidget::onTXSigned(unsigned int id, BinaryData signedTX, std::string error)
{
   if (payinSignId_ && (payinSignId_ == id)) {
      payinSignId_ = 0;
      if (!error.empty()) {
         ui_->labelHintPassword->setText(tr("Failed to create Pay-In TX - re-type password and try again"));
         logger_->error("[XBTSettlementTransactionWidget::onTXSigned] Failed to create pay-in TX: {}", error);
         retry();
         return;
      }
      stop();
      payinData_ = signedTX;
      payoutSignId_ = createPayoutTx(Tx(payinData_).getThisHash(), amount_, recvAddr_);
   } else if (payoutSignId_ && (payoutSignId_ == id)) {
      payoutSignId_ = 0;
      if (!error.empty()) {
         logger_->warn("[XBTSettlementTransactionWidget::onTXSigned] Pay-Out sign failure: {}", error);
         ui_->labelHintPassword->setText(tr("Pay-Out signing failed: %1").arg(QString::fromStdString(error)));
         retry();
         return;
      }
      payoutData_ = signedTX;
      if (!clientSells_) {
         transactionData_->GetWallet()->SetTransactionComment(payoutData_, comment_);
         walletsManager_->GetSettlementWallet()->SetTransactionComment(payoutData_, comment_);
      }

      ui_->labelHintPassword->setText(tr("Waiting for Order verification"));

      setupTimer();
      waitForPayout_ = waitForPayin_ = true;
      // send order
      quoteProvider_->AcceptQuote(QString::fromStdString(rfq_.requestId), quote_
         , payoutData_.toHexStr());

      expireTime_ = QDateTime::currentDateTime().addSecs(WaitTimeoutInSec);
      timer_.start();
   }
}

void XBTSettlementTransactionWidget::init(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<AuthAddressManager>& manager
   , const std::shared_ptr<AssetManager> &assetManager, const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<SignContainer> &container)
{
   logger_ = logger;
   authAddressManager_ = manager;
   assetManager_ = assetManager;
   quoteProvider_ = quoteProvider;
   signingContainer_ = container;

   utxoAdapter_ = std::make_shared<bs::UtxoReservation::Adapter>();
   bs::UtxoReservation::addAdapter(utxoAdapter_);

   connect(signingContainer_.get(), &SignContainer::TXSigned, this, &XBTSettlementTransactionWidget::onTXSigned);
}

void XBTSettlementTransactionWidget::payoutOnCancel()
{
   utxoAdapter_->unreserve(reserveId_);

   if (!PyBlockDataManager::instance()->broadcastZC(payoutData_)) {
      logger_->error("[XBTSettlementTransactionWidget::payoutOnCancel] failed to broadcast payout");
      return;
   }
   const std::string revokeComment = "Revoke for " + comment_;
   transactionData_->GetWallet()->SetTransactionComment(payoutData_, revokeComment);
   walletsManager_->GetSettlementWallet()->SetTransactionComment(payoutData_, revokeComment);
}

void XBTSettlementTransactionWidget::detectDealerTxs()
{
   if (clientSells_)  return;
   if (!waitForPayin_ && !waitForPayout_) {
      ui_->labelHintPassword->setText(tr("Both pay-in and pay-out transactions were detected!"));
      logger_->debug("[ XBTSettlementTransactionWidget::detectDealerTxs] Both pay-in and pay-out transactions were detected on requester side");
      timer_.stop();
      emit settlementAccepted();
   }
}

void XBTSettlementTransactionWidget::updateAcceptButton()
{
   bool passwordEntered = !ui_->lineEditPassword->text().isEmpty();
   if (clientSells_ && !sellFromPrimary_) {
      passwordEntered = passwordEntered && !ui_->lineEditAuthPassword->text().isEmpty();
   }

   ui_->pushButtonAccept->setEnabled(userKeyOk_
      && (dealerVerifState_ == AddressVerificationState::Verified)
      && passwordEntered);
}

void XBTSettlementTransactionWidget::onPayInZCDetected()
{
   if (waitForPayin_ && !settlementId_.isNull()) {
      waitForPayin_ = false;

      if (clientSells_) {
         waitForPayout_ = true;
         expireTime_ = QDateTime::currentDateTime().addSecs(WaitTimeoutInSec);
         timer_.start();
         ui_->labelHintPassword->setText(tr("Waiting for dealer's pay-out in blockchain..."));
      } else {
         detectDealerTxs();
      }
   }
}

void XBTSettlementTransactionWidget::onPayoutZCDetected(int confNum, bs::PayoutSigner::Type signedBy)
{
   Q_UNUSED(confNum);
   logger_->debug("[XBTSettlementTransactionWidget::onPayoutZCDetected] signedBy={}"
      , bs::PayoutSigner::toString(signedBy));
   if (waitForPayout_) {
      waitForPayout_ = false;
   }

   if (!settlementId_.isNull()) {
      if (clientSells_) {
         timer_.stop();
         emit settlementAccepted();
      } else {
         detectDealerTxs();
      }
   }
}

void XBTSettlementTransactionWidget::onZCError(const QString &txHash, const QString &errMsg)
{
   logger_->error("broadcastZC({}) error {}", txHash.toStdString(), errMsg.toStdString());
   ui_->labelHintPassword->setText(tr("Failed to broadcast TX: %1").arg(errMsg));
   emit settlementCancelled();
}

void XBTSettlementTransactionWidget::OrderReceived()
{
   if (clientSells_) {
      try {
         if (!PyBlockDataManager::instance()->broadcastZC(payinData_)) {
            throw std::runtime_error("Failed to bradcast transaction");
         }
         transactionData_->GetWallet()->SetTransactionComment(payinData_, comment_);
         walletsManager_->GetSettlementWallet()->SetTransactionComment(payinData_, comment_);

         waitForPayin_ = true;
         logger_->debug("[XBTSettlementTransactionWidget::OrderReceived] Pay-In broadcasted");
         ui_->labelHintPassword->setText(tr("Waiting for own pay-in in blockchain..."));
      }
      catch (const std::exception &e) {
         logger_->error("[XBTSettlementTransactionWidget::OrderReceived] Pay-In failed: {}", e.what());
         ui_->labelHintPassword->setText(tr("Sending of Pay-In failed: %1").arg(QString::fromStdString(e.what())));
      }
   } else {
      ui_->labelHintPassword->setText(tr("Waiting for dealer to broadcast both TXes to blockchain"));
   }
}
