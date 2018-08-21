#include "DealerCCSettlementDialog.h"
#include "ui_DealerCCSettlementDialog.h"

#include "ArmoryConnection.h"
#include "CommonTypes.h"
#include "DealerCCSettlementContainer.h"
#include "MetaData.h"
#include "UiUtils.h"
#include "WalletsManager.h"
#include "HDWallet.h"
#include <QtConcurrent/QtConcurrentRun>
#include <CelerClient.h>

#include <spdlog/spdlog.h>


DealerCCSettlementDialog::DealerCCSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<DealerCCSettlementContainer> &container
      , const std::string &reqRecvAddr
      , std::shared_ptr<WalletsManager> walletsManager
      , const std::shared_ptr<SignContainer> &signContainer
      , std::shared_ptr<CelerClient> celerClient
      , QWidget* parent)
   : BaseDealerSettlementDialog(logger, container, signContainer, parent)
   , ui_(new Ui::DealerCCSettlementDialog())
   , settlContainer_(container)
   , sValid(tr("<span style=\"color: #22C064;\">Verified</span>"))
   , sInvalid(tr("<span style=\"color: #CF292E;\">Invalid</span>"))
{
   ui_->setupUi(this);
   connectToProgressBar(ui_->progressBar);
   connectToHintLabel(ui_->labelHint, ui_->labelError);

   connect(celerClient.get(), &CelerClient::OnConnectionClosed,
      this, &DealerCCSettlementDialog::reject);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &DealerCCSettlementDialog::reject);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &DealerCCSettlementDialog::onAccepted);

   connect(settlContainer_.get(), &DealerCCSettlementContainer::genAddressVerified, this,
      &DealerCCSettlementDialog::onGenAddressVerified, Qt::QueuedConnection);
   connect(settlContainer_.get(), &DealerCCSettlementContainer::timerExpired, this,
      &DealerCCSettlementDialog::reject);
   connect(settlContainer_.get(), &DealerCCSettlementContainer::failed, this, &QDialog::reject);

   ui_->labelProductGroup->setText(QString::fromStdString(bs::network::Asset::toString(settlContainer_->assetType())));
   ui_->labelSecurityId->setText(QString::fromStdString(settlContainer_->security()));
   ui_->labelProduct->setText(QString::fromStdString(settlContainer_->product()));
   ui_->labelSide->setText( QString::fromStdString(bs::network::Side::toString(settlContainer_->side())));

   ui_->labelPrice->setText(UiUtils::displayPriceCC(settlContainer_->price()));

   ui_->labelQuantity->setText(UiUtils::displayCCAmount(settlContainer_->quantity()));

   ui_->labelTotal->setText(UiUtils::displayAmount(settlContainer_->quantity() * settlContainer_->price()));

   settlContainer_->activate();

   if (settlContainer_->isDelivery()) {
      setWindowTitle(tr("Settlement Delivery"));
      ui_->labelPaymentName->setText(tr("Delivery"));
      setFrejaPasswordPrompt(tr("%1 Delivery")
         .arg(QString::fromStdString(settlContainer_->security())));
   } else {
      setWindowTitle(tr("Settlement Payment"));
      ui_->labelPaymentName->setText(tr("Payment"));
      setFrejaPasswordPrompt(tr("%1 Payment")
         .arg(QString::fromStdString(settlContainer_->security())));
   }

   ui_->labelPayment->setText(settlContainer_->foundRecipAddr() && settlContainer_->isAmountValid()
      ? sValid : sInvalid);

   const auto &wallet = walletsManager->GetHDRootForLeaf(settlContainer_->GetSigningWallet()->GetWalletId());
   setWallet(wallet);
   ui_->labelPasswordHint->setText(tr("Enter password for \"%1\" wallet").arg(QString::fromStdString(wallet->getName())));

   validateGUI();
}

QWidget *DealerCCSettlementDialog::widgetPassword() const { return ui_->horizontalWidgetPassword; }
WalletKeysSubmitWidget *DealerCCSettlementDialog::widgetWalletKeys() const { return ui_->widgetSubmitKeys; }
QLabel *DealerCCSettlementDialog::labelHint() const { return ui_->labelHint; }
QLabel *DealerCCSettlementDialog::labelPassword() const { return ui_->labelPasswordHint; }

void DealerCCSettlementDialog::validateGUI()
{
   ui_->pushButtonAccept->setEnabled(settlContainer_->isAcceptable() && widgetWalletKeys()->isValid());
}

void DealerCCSettlementDialog::onGenAddressVerified(bool addressVerified)
{
   if (addressVerified) {
      ui_->labelGenesisAddress->setText(sValid);
      readyToAccept();
   } else {
      ui_->labelGenesisAddress->setText(sInvalid);
      widgetWalletKeys()->setEnabled(false);
   }
   validateGUI();
}

void DealerCCSettlementDialog::onAccepted()
{
   if (settlContainer_->accept(widgetWalletKeys()->key())) {
      ui_->pushButtonAccept->setEnabled(false);
   }
}
