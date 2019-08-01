#include "DealerCCSettlementDialog.h"
#include "ui_DealerCCSettlementDialog.h"

#include "ArmoryConnection.h"
#include "CommonTypes.h"
#include "DealerCCSettlementContainer.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include <CelerClient.h>

#include <spdlog/spdlog.h>


DealerCCSettlementDialog::DealerCCSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<DealerCCSettlementContainer> &container
      , const std::string &reqRecvAddr
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
      , const std::shared_ptr<SignContainer> &signContainer
      , std::shared_ptr<BaseCelerClient> celerClient
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , QWidget* parent)
   : BaseDealerSettlementDialog(logger, container, signContainer, appSettings, connectionManager, parent)
   , ui_(new Ui::DealerCCSettlementDialog())
   , settlContainer_(container)
   , sValid(tr("<span style=\"color: #22C064;\">Verified</span>"))
   , sInvalid(tr("<span style=\"color: #CF292E;\">Invalid</span>"))
{
   ui_->setupUi(this);
   connectToProgressBar(ui_->progressBar, ui_->labelTimeLeft);
   connectToHintLabel(ui_->labelHint, ui_->labelError);

   connect(celerClient.get(), &BaseCelerClient::OnConnectionClosed,
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
      setAuthPasswordPrompt(tr("%1 Delivery")
         .arg(QString::fromStdString(settlContainer_->security())));
   } else {
      setWindowTitle(tr("Settlement Payment"));
      ui_->labelPaymentName->setText(tr("Payment"));
      setAuthPasswordPrompt(tr("%1 Payment")
         .arg(QString::fromStdString(settlContainer_->security())));
   }

   ui_->labelPayment->setText(settlContainer_->foundRecipAddr() && settlContainer_->isAmountValid()
      ? sValid : sInvalid);

   if (!settlContainer_->GetSigningWallet()) {
      throw std::runtime_error("missing signing wallet in the container");
   }
   const auto &wallet = walletsManager->getHDRootForLeaf(settlContainer_->GetSigningWallet()->walletId());
   setWallet(wallet);
   ui_->labelPasswordHint->setText(tr("Enter password for \"%1\" wallet").arg(QString::fromStdString(wallet->name())));

   validateGUI();
}

DealerCCSettlementDialog::~DealerCCSettlementDialog() = default;

QWidget *DealerCCSettlementDialog::widgetPassword() const { return ui_->horizontalWidgetPassword; }
//WalletKeysSubmitWidget *DealerCCSettlementDialog::widgetWalletKeys() const { return ui_->widgetSubmitKeys; }
QLabel *DealerCCSettlementDialog::labelHint() const { return ui_->labelHint; }
QLabel *DealerCCSettlementDialog::labelPassword() const { return ui_->labelPasswordHint; }

void DealerCCSettlementDialog::validateGUI()
{
   //ui_->pushButtonAccept->setEnabled(settlContainer_->isAcceptable() && widgetWalletKeys()->isValid());
}

void DealerCCSettlementDialog::onGenAddressVerified(bool addressVerified)
{
   if (addressVerified) {
      ui_->labelGenesisAddress->setText(sValid);
      readyToAccept();
   } else {
      ui_->labelGenesisAddress->setText(sInvalid);
      //widgetWalletKeys()->setEnabled(false);
   }
   validateGUI();
}

void DealerCCSettlementDialog::onAccepted()
{
   // FIXME: this widget needs to be reimplemented to move signing to signer
//   if (settlContainer_->accept(widgetWalletKeys()->key())) {
//      ui_->pushButtonAccept->setEnabled(false);
//   }
}
