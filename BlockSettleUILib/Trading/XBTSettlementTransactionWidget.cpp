#include "XBTSettlementTransactionWidget.h"

#include "ui_XBTSettlementTransactionWidget.h"

#include "ReqXBTSettlementContainer.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include <CelerClient.h>

#include <spdlog/logger.h>


XBTSettlementTransactionWidget::XBTSettlementTransactionWidget(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<BaseCelerClient> &celerClient
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ReqXBTSettlementContainer> &settlContainer
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::XBTSettlementTransactionWidget())
   , logger_(logger)
   , appSettings_(appSettings)
   , settlContainer_(settlContainer)
   , connectionManager_(connectionManager)
   , sValid_(tr("<span style=\"color: #22C064;\">Verified</span>"))
   , sInvalid_(tr("<span style=\"color: #CF292E;\">Invalid</span>"))
   , sFailed_(tr("<span style=\"color: #CF292E;\">Failed</span>"))
{
   ui_->setupUi(this);

//   setupTimer();

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &XBTSettlementTransactionWidget::onCancel);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &XBTSettlementTransactionWidget::onAccept);

   //connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateAcceptButton(); });
   //connect(ui_->widgetSubmitKeysAuth, &WalletKeysSubmitWidget::keyChanged, [this] { updateAcceptButton(); });

   connect(celerClient.get(), &BaseCelerClient::OnConnectionClosed,
      this, &XBTSettlementTransactionWidget::onCancel);

   connect(settlContainer_.get(), &ReqXBTSettlementContainer::DealerVerificationStateChanged
      , this, &XBTSettlementTransactionWidget::onDealerVerificationStateChanged, Qt::QueuedConnection);
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::error, this, &XBTSettlementTransactionWidget::onError);
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::info, this, &XBTSettlementTransactionWidget::onInfo);
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::timerTick, this, &XBTSettlementTransactionWidget::onTimerTick);
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::timerExpired, this, &XBTSettlementTransactionWidget::onTimerExpired);
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::timerStarted, [this](int msDuration) { ui_->progressBar->setMaximum(msDuration); });
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::retry, this, &XBTSettlementTransactionWidget::onRetry);
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::stop, this, &XBTSettlementTransactionWidget::onStop);
   connect(settlContainer_.get(), &ReqXBTSettlementContainer::authWalletInfoReceived, this, &XBTSettlementTransactionWidget::onAuthWalletInfoReceived);

   settlContainer_->activate();

   populateDetails();
   ui_->pushButtonCancel->setEnabled(true);
   updateAcceptButton();
}

XBTSettlementTransactionWidget::~XBTSettlementTransactionWidget() noexcept = default;

void XBTSettlementTransactionWidget::onCancel()
{
   settlContainer_->cancel();
//   ui_->widgetSubmitKeys->cancel();
//   ui_->widgetSubmitKeysAuth->cancel();
}

void XBTSettlementTransactionWidget::onError(QString text)
{
   ui_->labelHintPassword->setText(text);
   updateAcceptButton();
}

void XBTSettlementTransactionWidget::onInfo(QString text)
{
   ui_->labelHintPassword->setText(text);
}

void XBTSettlementTransactionWidget::onTimerTick(int msCurrent, int)
{
   ui_->progressBar->setValue(msCurrent);
   ui_->progressBar->setFormat(tr("%n second(s) remaining", "", msCurrent / 1000));
   ui_->labelTimeLeft->setText(tr("%n second(s) remaining", "", msCurrent / 1000));
}

void XBTSettlementTransactionWidget::onTimerExpired()
{
   ui_->pushButtonCancel->setEnabled(false);
}

void XBTSettlementTransactionWidget::populateDetails()
{
   ui_->labelProductGroup->setText(tr(bs::network::Asset::toString(settlContainer_->assetType())));
   ui_->labelSecurityID->setText(QString::fromStdString(settlContainer_->security()));
   ui_->labelProduct->setText(QString::fromStdString(settlContainer_->product()));
   ui_->labelSide->setText(tr(bs::network::Side::toString(settlContainer_->side())));

   QString qtyProd = UiUtils::XbtCurrency;
   ui_->labelQuantity->setText(tr("%1 %2")
      .arg(UiUtils::displayAmountForProduct(settlContainer_->amount(), qtyProd, bs::network::Asset::Type::SpotXBT))
      .arg(qtyProd));

   ui_->labelPrice->setText(UiUtils::displayPriceXBT(settlContainer_->price()));

   const auto fxProd = QString::fromStdString(settlContainer_->fxProduct());
   ui_->labelTotalValue->setText(tr("%1 %2")
      .arg(UiUtils::displayAmountForProduct(settlContainer_->amount() * settlContainer_->price(), fxProd, bs::network::Asset::Type::SpotXBT))
      .arg(fxProd));

   populateXBTDetails();

   if (settlContainer_->weSell()) {
      if (settlContainer_->isSellFromPrimary()) {
         ui_->labelHintAuthPassword->hide();
         ui_->horizontalWidgetAuthPassword->hide();
         //ui_->widgetSubmitKeysAuth->suspend();
      }
      else {
         ui_->labelHintAuthPassword->setText(tr("Enter password for \"%1\" wallet to sign revoke Pay-Out")
            .arg(settlContainer_->walletInfoAuth().name()));
      }
   }
   else {
      ui_->labelHintPassword->setText(tr("Enter password for \"%1\" wallet to sign Pay-Out")
         .arg(settlContainer_->walletInfoAuth().name()));
      ui_->labelHintAuthPassword->hide();
      ui_->horizontalWidgetAuthPassword->hide();
      //ui_->widgetSubmitKeysAuth->suspend();
   }
}

void XBTSettlementTransactionWidget::onDealerVerificationStateChanged(AddressVerificationState state)
{
//   QString text;
//   switch (state) {
//   case AddressVerificationState::Verified: {
//         text = sValid_;
//         ui_->widgetSubmitKeys->init(AutheIDClient::SettlementTransaction, settlContainer_->walletInfo()
//            , WalletKeyWidget::UseType::RequestAuthInParent, logger_, appSettings_, connectionManager_);
//         ui_->widgetSubmitKeys->setFocus();
//         // tr("%1 Settlement %2").arg(QString::fromStdString(rfq_.security)).arg(clientSells_ ? tr("Pay-In") : tr("Pay-Out"))

//         if (settlContainer_->weSell() && !settlContainer_->isSellFromPrimary()) {
//            ui_->widgetSubmitKeysAuth->init(AutheIDClient::SettlementTransaction, settlContainer_->walletInfoAuth()
//            , WalletKeyWidget::UseType::RequestAuthInParent, logger_, appSettings_, connectionManager_);
//         }
//         QApplication::processEvents();
//         adjustSize();
//   }
//      break;
//   case AddressVerificationState::VerificationFailed:
//      text = sFailed_;
//      break;
//   default:
//      text = sInvalid_;
//      break;
//   }

//   ui_->labelDealerAuthAddress->setText(text);
//   updateAcceptButton();
}

void XBTSettlementTransactionWidget::onAuthWalletInfoReceived()
{
   //ui_->widgetSubmitKeysAuth->resume();
}

void XBTSettlementTransactionWidget::populateXBTDetails()
{
   ui_->labelDealerAuthAddress->setText(tr("Validating"));
   ui_->labelUserAuthAddress->setText(settlContainer_->userKeyOk() ? sValid_ : sInvalid_);

   if (settlContainer_->weSell()) {
      window()->setWindowTitle(tr("Settlement Pay-In (XBT)"));

      // addDetailRow(tr("Sending wallet"), tr("<b>%1</b> (%2)").arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletName()))
      //    .arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletId())));
      // addDetailRow(tr("Number of inputs"), tr("<b>%L1</b>")
      //    .arg(QString::number(transactionData_->GetTransactionSummary().usedTransactions)));

      ui_->labelHintPassword->setText(tr("Enter Password and press \"Accept\" to send Pay-In"));
   }
   else {
      window()->setWindowTitle(tr("Settlement Pay-Out (XBT)"));

      // addDetailRow(tr("Receiving wallet"), tr("<b>%1</b> (%2)").arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletName()))
      //    .arg(QString::fromStdString(transactionData_->GetWallet()->GetWalletId())));
      // addDetailRow(tr("Receiving address"), tr("<b>%1</b>").arg(recvAddr_.display()));

      ui_->labelHintPassword->setText(tr("Enter Password and press \"Accept\" to send Pay-Out to dealer"));
   }

   ui_->labelTransactionAmount->setText(UiUtils::displayQuantity(settlContainer_->amount(), UiUtils::XbtCurrency));
   ui_->labelFee->setText(UiUtils::displayQuantity(UiUtils::amountToBtc(settlContainer_->fee()), UiUtils::XbtCurrency));

   if (settlContainer_->weSell()) {
      ui_->labelTotalDescription->setText(tr("Total spent"));
      ui_->labelTotalAmount->setText(UiUtils::displayQuantity(settlContainer_->amount() + UiUtils::amountToBtc(settlContainer_->fee())
         , UiUtils::XbtCurrency));
   } else {
      ui_->labelTotalDescription->setText(tr("Total received"));
      ui_->labelTotalAmount->setText(UiUtils::displayQuantity(settlContainer_->amount() - UiUtils::amountToBtc(settlContainer_->fee())
         , UiUtils::XbtCurrency));
   }
}

void XBTSettlementTransactionWidget::onAccept()
{
   // FIXME: this widget needs to be reimplemented to move signing to signer

//   SecureBinaryData authKey = (settlContainer_->weSell() && !settlContainer_->isSellFromPrimary() && settlContainer_->payinReceived())
//      ? ui_->widgetSubmitKeysAuth->key() : ui_->widgetSubmitKeys->key();
//   settlContainer_->accept(authKey);

//   if (!settlContainer_->payinReceived() && !settlContainer_->weSell()) {
//      ui_->pushButtonCancel->setEnabled(false);
//   }
//   ui_->labelHintPassword->clear();
//   ui_->pushButtonAccept->setEnabled(false);
}

void XBTSettlementTransactionWidget::onStop()
{
//   ui_->progressBar->setValue(0);
//   ui_->widgetSubmitKeys->setEnabled(false);
//   ui_->labelHintAuthPassword->clear();
//   ui_->widgetSubmitKeysAuth->setEnabled(false);
}

void XBTSettlementTransactionWidget::onRetry()
{
//   ui_->pushButtonAccept->setText(tr("Retry"));
//   ui_->widgetSubmitKeys->setEnabled(true);
//   ui_->widgetSubmitKeysAuth->setEnabled(true);
//   ui_->pushButtonAccept->setEnabled(true);
}

void XBTSettlementTransactionWidget::updateAcceptButton()
{
//   bool isPasswordValid = ui_->widgetSubmitKeys->isValid();
//   if (settlContainer_->weSell() && !settlContainer_->isSellFromPrimary()) {
//      isPasswordValid = ui_->widgetSubmitKeysAuth->isValid();
//   }
//   ui_->pushButtonAccept->setEnabled(settlContainer_->isAcceptable() && isPasswordValid);
}
