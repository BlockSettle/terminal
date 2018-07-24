#include "DealerXBTSettlementDialog.h"
#include "ui_DealerXBTSettlementDialog.h"

#include "AddressVerificator.h"
#include "AssetManager.h"
#include "AuthAddressManager.h"
#include "DealerXBTSettlementContainer.h"
#include "QuoteProvider.h"
#include "SettlementContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "WalletsManager.h"
#include "HDWallet.h"

#include <spdlog/spdlog.h>


DealerXBTSettlementDialog::DealerXBTSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<DealerXBTSettlementContainer> &settlContainer
      , const std::shared_ptr<AssetManager>& assetManager
      , std::shared_ptr<WalletsManager> walletsManager
      , const std::shared_ptr<SignContainer> &signContainer
      , QWidget* parent)
   : BaseDealerSettlementDialog(logger, settlContainer, signContainer, parent)
   , ui_(new Ui::DealerXBTSettlementDialog())
   , settlContainer_(settlContainer)
{
   ui_->setupUi(this);

   connectToProgressBar(ui_->progressBar);
   connectToHintLabel(ui_->labelHint, ui_->labelError);

   ui_->labelProductGroup->setText(QString::fromStdString(bs::network::Asset::toString(settlContainer_->assetType())));
   ui_->labelSecurityId->setText(QString::fromStdString(settlContainer_->security()));
   ui_->labelProduct->setText(QString::fromStdString(settlContainer_->product()));
   ui_->labelSide->setText(QString::fromStdString(bs::network::Side::toString(settlContainer_->side())));
   ui_->labelQuantity->setText(UiUtils::displayQuantity(settlContainer_->amount(), UiUtils::XbtCurrency));

   ui_->labelPrice->setText(UiUtils::displayPriceXBT(settlContainer_->price()));

   ui_->labelTotal->setText(UiUtils::displayCurrencyAmount(settlContainer_->amount() * settlContainer_->price()));

   const auto &wallet = walletsManager->GetHDRootForLeaf(settlContainer_->GetWallet()->GetWalletId());
   ui_->labelWallet->setText(QString::fromStdString(wallet->getName()));
   setWallet(wallet);

   if (settlContainer_->weSell()) {
      ui_->labelTransactionDescription->setText(tr("Deliver"));
      ui_->labelTransactioAmount->setText(UiUtils::displayQuantity(settlContainer_->amount(), UiUtils::XbtCurrency));
      setFrejaPasswordPrompt(tr("%1 Settlement %2")
         .arg(QString::fromStdString(settlContainer_->security())).arg(tr("Pay-In")));
   } else {
      ui_->labelTransactionDescription->setText(tr("Receive"));
      ui_->labelTransactioAmount->setText(tr("Waiting for pay-in TX"));
      setFrejaPasswordPrompt(tr("%1 Settlement %2")
         .arg(QString::fromStdString(settlContainer_->security())).arg(tr("Pay-Out")));
   }

   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &DealerXBTSettlementDialog::onAccepted);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &DealerXBTSettlementDialog::reject);

   connect(settlContainer_.get(), &DealerXBTSettlementContainer::cptyAddressStateChanged, this
      , &DealerXBTSettlementDialog::onRequestorAddressStateChanged);
   connect(settlContainer_.get(), &DealerXBTSettlementContainer::timerExpired, this, &DealerXBTSettlementDialog::onTimerExpired);
   connect(settlContainer_.get(), &DealerXBTSettlementContainer::failed, this, &DealerXBTSettlementDialog::onSettlementFailed);
   connect(settlContainer_.get(), &DealerXBTSettlementContainer::payInDetected, this, &DealerXBTSettlementDialog::payInDetected);

   connect(settlContainer_.get(), &DealerXBTSettlementContainer::info, this
      , &DealerXBTSettlementDialog::onInfoFromContainer, Qt::QueuedConnection);
   connect(settlContainer_.get(), &DealerXBTSettlementContainer::error, this
      , &DealerXBTSettlementDialog::onErrorFromContainer, Qt::QueuedConnection);

   ui_->pushButtonAccept->setEnabled(false);

   if (!settlContainer_->weSell()) {
      // we should wait for payin from customer before accept
      ui_->lineEditPassword->setEnabled(false);
   }

   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &DealerXBTSettlementDialog::updateControls);

   activate();
}

QWidget *DealerXBTSettlementDialog::widgetPassword() const { return ui_->horizontalWidgetPassword; }
QLineEdit *DealerXBTSettlementDialog::lineEditPassword() const { return ui_->lineEditPassword; }
QLabel *DealerXBTSettlementDialog::labelHint() const { return ui_->labelHint; }
QLabel *DealerXBTSettlementDialog::labelPassword() const { return ui_->labelError; }

void DealerXBTSettlementDialog::onRequestorAddressStateChanged(AddressVerificationState state)
{
   const auto formatString = tr("<span style=\"color: %1;\">%2</span>");
   QString color;
   QString stateText;

   switch (state)
   {
   case AddressVerificationState::VerificationFailed:
      color = QString::fromStdString("#CF292E");
      stateText = tr("Verification failed");
      break;
   case AddressVerificationState::InProgress:
      color = QString::fromStdString("#F7B03A");
      stateText = tr("In progress");
      break;
   case AddressVerificationState::NotSubmitted:
      color = QString::fromStdString("#CF292E");
      stateText = tr("Not submitted to BS");
      break;
   case AddressVerificationState::Submitted:
   case AddressVerificationState::PendingVerification:
   case AddressVerificationState::VerificationSubmitted:
      color = QString::fromStdString("#CF292E");
      stateText = tr("Not verified");
      break;
   case AddressVerificationState::Verified:
      color = QString::fromStdString("#22C064");
      stateText = tr("Verified");
      break;
   case AddressVerificationState::Revoked:
      color = QString::fromStdString("#CF292E");
      stateText = tr("Revoked");
      break;
   case AddressVerificationState::RevokedByBS:
      color = QString::fromStdString("#CF292E");
      stateText = tr("Revoked by BS");
      break;
   }

   ui_->labelRequestorAuthAddress->setText(formatString.arg(color).arg(stateText));

   if (state == AddressVerificationState::InProgress) {
      setHintText(tr("Waiting for requestor auth address verification"));
   } else if (state == AddressVerificationState::NotSubmitted) {
      setHintText(tr("Requestor auth address wasn't submitted for verification"));
   } else if (state != AddressVerificationState::Verified) {
      setCriticalHintMessage(tr("Requestor auth address could not be accepted"));
      deactivate();
   } else {
      updateControls();
   }
}

void DealerXBTSettlementDialog::updateControls()
{
   if (settlContainer_->isAcceptable() && !acceptable_) {
      acceptable_ = true;
      readyToAccept();
   }
   ui_->lineEditPassword->setEnabled(acceptable_);
   ui_->pushButtonAccept->setEnabled(acceptable_ && !walletPassword_.isNull());
}

void DealerXBTSettlementDialog::onTimerExpired()
{
   setCriticalHintMessage(tr("Timer expired"));
   deactivate();
   updateControls();
   reject();
}

void DealerXBTSettlementDialog::activate()
{
   setWindowTitle(tr("Settlement %1 (XBT)").arg(settlContainer_->weSell() ? tr("Pay-In") : tr("Pay-Out")));
   settlContainer_->activate();
}

void DealerXBTSettlementDialog::deactivate()
{
   settlContainer_->deactivate();
}

void DealerXBTSettlementDialog::payInDetected(int confirmationsNumber, const BinaryData &txHash)
{
   ui_->labelTransactioAmount->setText(UiUtils::displayQuantity(settlContainer_->amount() - UiUtils::amountToBtc(settlContainer_->fee())
      , UiUtils::XbtCurrency));

   ui_->lineEditPassword->setEnabled(!settlContainer_->weSell());

   updateControls();
}

void DealerXBTSettlementDialog::onInfoFromContainer(const QString &text)
{
   setHintText(text);
}

void DealerXBTSettlementDialog::onErrorFromContainer(const QString &text)
{
   setCriticalHintMessage(text);
   updateControls();
}

void DealerXBTSettlementDialog::disableCancelOnOrder()
{
   ui_->pushButtonCancel->setEnabled(false);
}

void DealerXBTSettlementDialog::onAccepted()
{
   ui_->pushButtonCancel->setEnabled(false);

   disableCancelOnOrder();
   setHintText(tr("Waiting for transactions signing..."));

   ui_->lineEditPassword->setEnabled(false);
   settlContainer_->accept(walletPassword_);

   updateControls();
}

void DealerXBTSettlementDialog::onSettlementFailed()
{
   setHintText(tr("You can retry signing with another password"));
   ui_->lineEditPassword->clear();
   ui_->pushButtonAccept->setText(tr("Retry"));
   updateControls();
}
