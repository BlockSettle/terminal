#include "CCSettlementTransactionWidget.h"
#include "ui_CCSettlementTransactionWidget.h"

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "CheckRecipSigner.h"
#include "SignContainer.h"
#include "QuoteProvider.h"
#include "SelectedTransactionInputs.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "CelerClient.h"
#include "ReqCCSettlementContainer.h"

#include <QLabel>

#include <spdlog/logger.h>

CCSettlementTransactionWidget::~CCSettlementTransactionWidget() noexcept = default;

CCSettlementTransactionWidget::CCSettlementTransactionWidget(
   const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<BaseCelerClient> &celerClient
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ReqCCSettlementContainer> &settlContainer
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::CCSettlementTransactionWidget())
   , logger_(logger)
   , appSettings_(appSettings)
   , settlContainer_(settlContainer)
   , connectionManager_(connectionManager)
   , sValid_(tr("<span style=\"color: #22C064;\">Verified</span>"))
   , sInvalid_(tr("<span style=\"color: #CF292E;\">Invalid</span>"))
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CCSettlementTransactionWidget::onCancel);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &CCSettlementTransactionWidget::onAccept);
   connect(settlContainer_.get(), &ReqCCSettlementContainer::genAddrVerified, this, &CCSettlementTransactionWidget::onGenAddrVerified, Qt::QueuedConnection);
   connect(settlContainer_.get(), &ReqCCSettlementContainer::paymentVerified, this, &CCSettlementTransactionWidget::onPaymentVerified, Qt::QueuedConnection);
   connect(settlContainer_.get(), &ReqCCSettlementContainer::error, this, &CCSettlementTransactionWidget::onError);
   connect(settlContainer_.get(), &ReqCCSettlementContainer::info, this, &CCSettlementTransactionWidget::onInfo);
   connect(settlContainer_.get(), &ReqCCSettlementContainer::timerTick, this, &CCSettlementTransactionWidget::onTimerTick);
   connect(settlContainer_.get(), &ReqCCSettlementContainer::timerExpired, this, &CCSettlementTransactionWidget::onTimerExpired);
   connect(settlContainer_.get(), &ReqCCSettlementContainer::timerStarted, [this](int msDuration) { ui_->progressBar->setMaximum(msDuration); });
   connect(settlContainer_.get(), &ReqCCSettlementContainer::walletInfoReceived, this, &CCSettlementTransactionWidget::initSigning);
   connect(celerClient.get(), &BaseCelerClient::OnConnectionClosed, this, &CCSettlementTransactionWidget::onCancel);
   //connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, this, &CCSettlementTransactionWidget::onKeyChanged);

   settlContainer_->activate();

   ui_->pushButtonCancel->setEnabled(true);
   populateDetails();
}

void CCSettlementTransactionWidget::onCancel()
{
   settlContainer_->cancel();
   //ui_->widgetSubmitKeys->cancel();
}

void CCSettlementTransactionWidget::onTimerTick(int msCurrent, int)
{
   ui_->progressBar->setValue(msCurrent);
   ui_->progressBar->setFormat(tr("%n second(s) remaining", "", msCurrent / 1000));
}

void CCSettlementTransactionWidget::onTimerExpired()
{
   onCancel();
}

void CCSettlementTransactionWidget::populateDetails()
{
   ui_->labelProductGroup->setText(tr(bs::network::Asset::toString(settlContainer_->assetType())));
   ui_->labelSecurityID->setText(QString::fromStdString(settlContainer_->security()));
   ui_->labelProduct->setText(QString::fromStdString(settlContainer_->product()));
   ui_->labelSide->setText(tr(bs::network::Side::toString(settlContainer_->side())));

   ui_->labelQuantity->setText(tr("%1 %2")
      .arg(UiUtils::displayCCAmount(settlContainer_->quantity()))
      .arg(QString::fromStdString(settlContainer_->product())));
   ui_->labelPrice->setText(UiUtils::displayPriceCC(settlContainer_->price()));

   ui_->labelTotalValue->setText(tr("%1").arg(UiUtils::displayAmount(settlContainer_->amount())));

   if (settlContainer_->side() == bs::network::Side::Sell) {
      window()->setWindowTitle(tr("Settlement Delivery"));
      ui_->labelPaymentName->setText(tr("Delivery"));
   }
   else {
      window()->setWindowTitle(tr("Settlement Payment"));
      ui_->labelPaymentName->setText(tr("Payment"));
   }

   // addDetailRow(tr("Receipt address"), QString::fromStdString(dealerAddress_));
   ui_->labelGenesisAddress->setText(tr("Verifying"));

   ui_->labelPasswordHint->setText(tr("Enter \"%1\" wallet password to accept")
      .arg(settlContainer_->walletInfo().name()));

   updateAcceptButton();
}

void CCSettlementTransactionWidget::onGenAddrVerified(bool result, QString error)
{
   logger_->debug("[CCSettlementTransactionWidget::onGenAddrVerified] result = {} ({})", result, error.toStdString());
   ui_->labelGenesisAddress->setText(result ? sValid_ : sInvalid_);
   updateAcceptButton();

   if (!result) {
      ui_->labelHint->setText(tr("Failed to verify genesis address: %1").arg(error));
   } else {
      ui_->labelHint->setText(tr("Accept offer to send your own signed half of the CoinJoin transaction"));
      initSigning();
   }
}

void CCSettlementTransactionWidget::initSigning()
{
   if (settlContainer_->walletInfo().encTypes().empty() || !settlContainer_->walletInfo().keyRank().first
      || !settlContainer_->isAcceptable()) {
      return;
   }

//   ui_->widgetSubmitKeys->init(AutheIDClient::SettlementTransaction, settlContainer_->walletInfo()
//      , WalletKeyWidget::UseType::RequestAuthInParent, logger_, appSettings_, connectionManager_);

//   ui_->widgetSubmitKeys->setFocus();
//   ui_->widgetSubmitKeys->resume();
}

void CCSettlementTransactionWidget::onPaymentVerified(bool result, QString error)
{
   if (!error.isEmpty()) {
      ui_->labelHint->setText(error);
   }
   ui_->labelPayment->setText(result ? sValid_ : sInvalid_);
   updateAcceptButton();
}

void CCSettlementTransactionWidget::onError(QString text)
{
   ui_->labelHint->setText(text);
   updateAcceptButton();
}

void CCSettlementTransactionWidget::onInfo(QString text)
{
   ui_->labelHint->setText(text);
}

void CCSettlementTransactionWidget::onKeyChanged()
{
//   updateAcceptButton();
//   if (ui_->widgetSubmitKeys->isKeyFinal()) {
//      onAccept();
//   }
}

void CCSettlementTransactionWidget::onAccept()
{
   ui_->progressBar->setValue(0);
   ui_->labelHint->clear();
   ui_->pushButtonAccept->setEnabled(false);
   //ui_->widgetSubmitKeys->setEnabled(false);

   ui_->pushButtonCancel->setEnabled(false);

   // FIXME: this widget needs to be reimplemented to move signing to signer
   //settlContainer_->accept(ui_->widgetSubmitKeys->key());
}

void CCSettlementTransactionWidget::updateAcceptButton()
{
//   ui_->pushButtonAccept->setEnabled(settlContainer_->isAcceptable()
//      && ui_->widgetSubmitKeys->isValid());
}
