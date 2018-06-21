#include "DealerCCSettlementDialog.h"
#include "ui_DealerCCSettlementDialog.h"

#include "CommonTypes.h"
#include "DealerCCSettlementContainer.h"
#include "MetaData.h"
#include "PyBlockDataManager.h"
#include "UiUtils.h"
#include <QtConcurrent/QtConcurrentRun>

#include <spdlog/spdlog.h>


DealerCCSettlementDialog::DealerCCSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<DealerCCSettlementContainer> &container
      , const std::string &reqRecvAddr, QWidget* parent)
   : BaseDealerSettlementDialog(logger, container, parent)
   , ui_(new Ui::DealerCCSettlementDialog())
   , settlContainer_(container)
{
   ui_->setupUi(this);
   connectToProgressBar(ui_->progressBar);
   connectToHintLabel(ui_->labelHint, ui_->labelError);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &DealerCCSettlementDialog::reject);
   connect(ui_->pushButtonAccept, &QPushButton::clicked, this, &DealerCCSettlementDialog::onAccepted);

   connect(settlContainer_.get(), &DealerCCSettlementContainer::genAddressVerified, this,
      &DealerCCSettlementDialog::onGenAddressVerified, Qt::QueuedConnection);
   connect(settlContainer_.get(), &DealerCCSettlementContainer::timerExpired, this,
      &DealerCCSettlementDialog::reject);
   connect(settlContainer_.get(), &DealerCCSettlementContainer::completed, this, &QDialog::accept);
   connect(settlContainer_.get(), &DealerCCSettlementContainer::failed, this, &QDialog::reject);

   ui_->labelProductGroup->setText(QString::fromStdString(bs::network::Asset::toString(settlContainer_->assetType())));
   ui_->labelSecurityId->setText(QString::fromStdString(settlContainer_->security()));
   ui_->labelProduct->setText(QString::fromStdString(settlContainer_->product()));
   ui_->labelSide->setText( QString::fromStdString(bs::network::Side::toString(settlContainer_->side())));

   ui_->labelPrice->setText(UiUtils::displayPriceCC(settlContainer_->price()));

   ui_->labelQuantity->setText(UiUtils::displayCCAmount(settlContainer_->quantity()));

   ui_->labelTotal->setText(UiUtils::displayAmount(settlContainer_->quantity() * settlContainer_->price()));

   settlContainer_->activate();

   const auto strValid = tr("<span style=\"color: #22C064;\">Verified</span>");
   const auto strInvalid = tr("<span style=\"color: #CF292E;\">Invalid</span>");
   ui_->labelCounterpartyTx->setText(settlContainer_->foundRecipAddr() ? strValid : strInvalid);

   ui_->labelPasswordHint->setText(tr("Enter password for \"%1\" wallet").arg(settlContainer_->GetSigningWalletName()));

   ui_->verticalWidgetPassword->hide();
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &DealerCCSettlementDialog::validateGUI);

   validateGUI();
}

void DealerCCSettlementDialog::validateGUI()
{
   ui_->pushButtonAccept->setEnabled(settlContainer_->isAcceptable() && !ui_->lineEditPassword->text().isEmpty());
}

void DealerCCSettlementDialog::onGenAddressVerified(bool addressVerified)
{
   if (addressVerified) {
      ui_->labelGenesisAddress->setText(tr("<b><span style=\"color: darkGreen;\">valid</span></b>"));
      ui_->lineEditPassword->setEnabled(true);
      ui_->verticalWidgetPassword->show();
   } else {
      ui_->labelGenesisAddress->setText(tr("<b><span style=\"color: red;\">invalid</span></b>"));
      ui_->lineEditPassword->setEnabled(false);
   }
   validateGUI();
}

void DealerCCSettlementDialog::onAccepted()
{
   if (settlContainer_->accept(ui_->lineEditPassword->text().toStdString())) {
      ui_->pushButtonAccept->setEnabled(false);
   }
}

void DealerCCSettlementDialog::reject()
{
   settlContainer_->cancel();
   QDialog::reject();
}
