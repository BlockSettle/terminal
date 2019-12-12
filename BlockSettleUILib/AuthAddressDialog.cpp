/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AuthAddressDialog.h"
#include "ui_AuthAddressDialog.h"

#include <spdlog/spdlog.h>
#include <QItemSelection>

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "AuthAddressConfirmDialog.h"
#include "AuthAddressManager.h"
#include "AuthAddressViewModel.h"
#include "BSMessageBox.h"
#include "UiUtils.h"

AuthAddressDialog::AuthAddressDialog(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<AssetManager> &assetMgr
   , const std::shared_ptr<ApplicationSettings> &settings, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::AuthAddressDialog())
   , logger_(logger)
   , authAddressManager_(authAddressManager)
   , assetManager_(assetMgr)
   , settings_(settings)
{
   ui_->setupUi(this);

   model_ = new AuthAddressViewModel(authAddressManager_, ui_->treeViewAuthAdress);
   ui_->treeViewAuthAdress->setModel(model_);
   ui_->treeViewAuthAdress->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui_->treeViewAuthAdress->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &AuthAddressDialog::adressSelected);
   connect(model_, &AuthAddressViewModel::modelReset, this, &AuthAddressDialog::onModelReset);


   connect(authAddressManager_.get(), &AuthAddressManager::AddressListUpdated, this, &AuthAddressDialog::onAddressListUpdated, Qt::QueuedConnection);
   connect(authAddressManager_.get(), &AuthAddressManager::AddrStateChanged, this, &AuthAddressDialog::onAddressStateChanged, Qt::QueuedConnection);
   connect(authAddressManager_.get(), &AuthAddressManager::Error, this, &AuthAddressDialog::onAuthMgrError, Qt::QueuedConnection);
   connect(authAddressManager_.get(), &AuthAddressManager::Info, this, &AuthAddressDialog::onAuthMgrInfo, Qt::QueuedConnection);
   connect(authAddressManager_.get(), &AuthAddressManager::AuthAddressConfirmationRequired, this, &AuthAddressDialog::onAuthAddressConfirmationRequired, Qt::QueuedConnection);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &AuthAddressDialog::createAddress);
   connect(ui_->pushButtonRevoke, &QPushButton::clicked, this, &AuthAddressDialog::revokeSelectedAddress);
   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &AuthAddressDialog::submitSelectedAddress);
   connect(ui_->pushButtonDefault, &QPushButton::clicked, this, &AuthAddressDialog::setDefaultAddress);
}

AuthAddressDialog::~AuthAddressDialog() = default;

void AuthAddressDialog::showEvent(QShowEvent *evt)
{
   if (defaultAddr_.isNull()) {
      defaultAddr_ = authAddressManager_->getDefault();
      if (defaultAddr_.isNull() && authAddressManager_->GetAddressCount()) {
         defaultAddr_ = authAddressManager_->GetAddress(0);
      }
      model_->setDefaultAddr(defaultAddr_);
   }

   updateUnsubmittedState();

   ui_->treeViewAuthAdress->selectionModel()->clearSelection();

   ui_->labelHint->clear();

   ui_->pushButtonCreate->setEnabled(authAddressManager_->HaveAuthWallet());
   //ui_->pushButtonCreate->setEnabled(!unsubmittedExist());

   resizeTreeViewColumns();

   QDialog::showEvent(evt);
}

bool AuthAddressDialog::unsubmittedExist() const
{
   return unconfirmedExists_;
}

void AuthAddressDialog::updateUnsubmittedState()
{
   for (size_t i = 0; i < authAddressManager_->GetAddressCount(); i++) {
      const auto authAddr = authAddressManager_->GetAddress(i);
      if (!authAddressManager_->hasSettlementLeaf(authAddr)) {
         authAddressManager_->createSettlementLeaf(authAddr, [] {});
         unconfirmedExists_ = true;
         return;
      }
      switch (authAddressManager_->GetState(authAddr)) {
      case AddressVerificationState::NotSubmitted:
         //ui_->labelHint->setText(tr("There are unsubmitted addresses - adding new ones is temporarily suspended"));
         [[clang::fallthrough]];
      case AddressVerificationState::InProgress:
      case AddressVerificationState::VerificationFailed:
         unconfirmedExists_ = true;
         break;
      default:
         unconfirmedExists_ = false;
         break;
      }
   }
}

void AuthAddressDialog::onAuthMgrError(const QString &details)
{
   showError(tr("Authentication Address error"), details);
}

void AuthAddressDialog::onAuthMgrInfo(const QString &text)
{
   showInfo(tr("Authentication Address"), text);
}

void AuthAddressDialog::showError(const QString &text, const QString &details)
{
   BSMessageBox errorMessage(BSMessageBox::critical, tr("Error"), text, details, this);
   errorMessage.exec();
}

void AuthAddressDialog::showInfo(const QString &title, const QString &text)
{
   BSMessageBox(BSMessageBox::info, title, text).exec();
}

void AuthAddressDialog::setAddressToVerify(const QString &addr)
{
   if (addr.isEmpty()) {
      setWindowTitle(tr("Authentication Addresses"));
   } else {
      setWindowTitle(tr("Address %1 requires verification").arg(addr));
      for (int i = 0; i < model_->rowCount(); i++) {
         if (addr == model_->data(model_->index(i, 0))) {
            const auto &index = model_->index(i, 0);
            ui_->treeViewAuthAdress->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            ui_->treeViewAuthAdress->scrollTo(index);
            break;
         }
      }

      ui_->labelHint->setText(tr("Your Authentication Address can now be Verified. Please press <b>Verify</b> and enter your password to execute the address verification."));
      ui_->treeViewAuthAdress->setFocus();
   }
}

void AuthAddressDialog::setBsClient(BsClient *bsClient)
{
   bsClient_ = bsClient;
}

void AuthAddressDialog::onAddressListUpdated()
{
   updateUnsubmittedState();
   // BST-2237 - allow to create addreses in line
   //ui_->pushButtonCreate->setEnabled(!unsubmittedExist());
}

void AuthAddressDialog::onAuthVerifyTxSent()
{
   BSMessageBox(BSMessageBox::info, tr("Authentication Address")
      , tr("Verification Transaction successfully sent.")
      , tr("Once the validation transaction is mined six (6) blocks deep, your Authentication Address will be"
         " accepted as valid by the network and you can enter orders in the Spot XBT product group.")).exec();
   accept();
}

void AuthAddressDialog::onAddressStateChanged(const QString &addr, const QString &state)
{
   if (state == QLatin1String("Verified")) {
      BSMessageBox(BSMessageBox::success, tr("Authentication Address")
         , tr("Authentication Address verified")
         , tr("You may now place orders in the Spot XBT product group.")
         ).exec();
   } else if (state == QLatin1String("Revoked")) {
      BSMessageBox(BSMessageBox::warning, tr("Authentication Address")
         , tr("Authentication Address revoked")
         , tr("Authentication Address %1 was revoked and could not be used for Spot XBT trading.").arg(addr)).exec();
   }
}

void AuthAddressDialog::resizeTreeViewColumns()
{
   if (!ui_->treeViewAuthAdress->model()) {
      return;
   }

   for (int i = ui_->treeViewAuthAdress->model()->columnCount() - 1; i >= 0; --i) {
      ui_->treeViewAuthAdress->resizeColumnToContents(i);
   }
}

void AuthAddressDialog::adressSelected(const QItemSelection &selected, const QItemSelection &deselected)
{
   ui_->pushButtonCreate->setEnabled(true);
   ui_->pushButtonCreate->setFlat(true);

   Q_UNUSED(deselected)
   if (!selected.indexes().isEmpty()) {
      const auto address = model_->getAddress(selected.indexes()[0]);

      switch (authAddressManager_->GetState(address)) {
         case AddressVerificationState::NotSubmitted:
         case AddressVerificationState::VerificationFailed: // FIXME: temporarily
         case AddressVerificationState::InProgress:         // FIXME: temporarily
            ui_->pushButtonRevoke->setEnabled(false);
            ui_->pushButtonSubmit->setEnabled(lastSubmittedAddress_.isNull());
            ui_->pushButtonDefault->setEnabled(false);
            break;
         case AddressVerificationState::Submitted:
         case AddressVerificationState::Revoked:
         case AddressVerificationState::PendingVerification:
            ui_->pushButtonRevoke->setEnabled(false);
            ui_->pushButtonSubmit->setEnabled(false);
            ui_->pushButtonDefault->setEnabled(false);
            break;
         case AddressVerificationState::Verified:
            ui_->pushButtonRevoke->setEnabled(authAddressManager_->readyError() != AuthAddressManager::ReadyError::NoError);
            ui_->pushButtonSubmit->setEnabled(false);
            ui_->pushButtonDefault->setEnabled(address != defaultAddr_);
            break;
         default:
            break;
      }
   }
   else {
      ui_->pushButtonRevoke->setEnabled(false);
      ui_->pushButtonSubmit->setEnabled(false);
      ui_->pushButtonDefault->setEnabled(false);
   }
}

bs::Address AuthAddressDialog::GetSelectedAddress() const
{
   auto selectedRows = ui_->treeViewAuthAdress->selectionModel()->selectedRows();
   if (!selectedRows.isEmpty()) {
      return model_->getAddress(selectedRows[0]);
   }

  return bs::Address();
}

void AuthAddressDialog::createAddress()
{
   if (!authAddressManager_->CreateNewAuthAddress()) {
      showError(tr("Failed to create new address"), tr("Auth wallet error"));
   } else {
      ui_->pushButtonCreate->setEnabled(false);
   }
}

void AuthAddressDialog::revokeSelectedAddress()
{
   auto selectedAddress = GetSelectedAddress();
   if (selectedAddress.isNull()) {
      return;
   }

   BSMessageBox revokeQ(BSMessageBox::question, tr("Authentication Address")
      , tr("Revoke Authentication Address")
      , tr("Your Authentication Address\n"
           "%1\n"
           "will be revoked. Are you sure you wish to continue?")
         .arg(QString::fromStdString(selectedAddress.display()))
      , this);
   if (revokeQ.exec() == QDialog::Accepted) {
      authAddressManager_->RevokeAddress(selectedAddress);
   }
}

void AuthAddressDialog::onAuthAddressConfirmationRequired(float validationAmount)
{
   const auto eurBalance = assetManager_->getBalance("EUR");
   if (validationAmount > eurBalance) {
      BSMessageBox warnFunds(BSMessageBox::warning, tr("Insufficient EUR Balance")
         , tr("Please fund your EUR account prior to submitting an Authentication Address")
         , tr("Required amount (EUR): %1<br/>Deposits and withdrawals are administered through the "
            "<a href=\"https://blocksettle.com\"><span style=\"text-decoration: underline;color:%2;\">Client Portal</span></a>")
         .arg(UiUtils::displayCurrencyAmount(validationAmount))
         .arg(BSMessageBox::kUrlColor), this);
      warnFunds.setWindowTitle(tr("Insufficient Funds"));
      warnFunds.exec();

      authAddressManager_->CancelSubmitForVerification(bsClient_.data(), lastSubmittedAddress_);
      lastSubmittedAddress_ = bs::Address{};

      return;
   }

   int promptResult = 0;
   const auto &qryTitle = tr("Authentication Address");
   const auto &qryText = tr("New Authentication Address");
   if (validationAmount > 0) {
      promptResult = BSMessageBox(BSMessageBox::question, qryTitle, qryText
         , tr("Are you sure you wish to submit a new authentication address? Setting up a new Authentication Address"
            " costs %1 %2").arg(QLatin1String("EUR")).arg(UiUtils::displayCurrencyAmount(validationAmount))
         , tr("BlockSettle will not deduct an amount higher than the Fee Schedule maximum regardless of the"
            " stated cost. Please confirm BlockSettle can debit the Authentication Address fee from your account."), this).exec();
   }
   else {
      promptResult = BSMessageBox(BSMessageBox::question, qryTitle, qryText
         , tr("Are you sure you wish to submit a new authentication address?")
         , tr("It appears that you're submitting the same Authentication Address again. The confirmation is"
            " formal and won't result in any withdrawals from your account."), this).exec();
   }

   if (promptResult == QDialog::Accepted) {
      ConfirmAuthAddressSubmission();
   } else {
      authAddressManager_->CancelSubmitForVerification(bsClient_.data(), lastSubmittedAddress_);
      lastSubmittedAddress_ = bs::Address{};
   }
}

void AuthAddressDialog::ConfirmAuthAddressSubmission()
{
   if (!bsClient_) {
      SPDLOG_LOGGER_ERROR(logger_, "bsClient_ in not set");
      return;
   }

   AuthAddressConfirmDialog confirmDlg{bsClient_.data(), lastSubmittedAddress_, authAddressManager_, this};

   confirmDlg.exec();

   lastSubmittedAddress_ = bs::Address{};
}

void AuthAddressDialog::submitSelectedAddress()
{
   ui_->pushButtonSubmit->setEnabled(false);
   ui_->labelHint->clear();

   lastSubmittedAddress_ = GetSelectedAddress();
   if (lastSubmittedAddress_.isNull()) {
      return;
   }

   if (authAddressManager_->hasSettlementLeaf(lastSubmittedAddress_)) {
      authAddressManager_->SubmitForVerification(lastSubmittedAddress_);
   }
   else {
      authAddressManager_->createSettlementLeaf(lastSubmittedAddress_, [this] {
         QMetaObject::invokeMethod(this, [this] {  // prevent crash if dialog was destroyed
            authAddressManager_->SubmitForVerification(lastSubmittedAddress_);
         });
      });
   }
}

void AuthAddressDialog::setDefaultAddress()
{
   auto selectedAddress = GetSelectedAddress();
   if (!selectedAddress.isNull()) {
      defaultAddr_ = selectedAddress;
      settings_->set(ApplicationSettings::defaultAuthAddr
         , QString::fromStdString(defaultAddr_.display()));
      settings_->SaveSettings();
      model_->setDefaultAddr(defaultAddr_);
      authAddressManager_->setDefault(defaultAddr_);
      resizeTreeViewColumns();
   }
}

void AuthAddressDialog::onModelReset()
{
   ui_->pushButtonRevoke->setEnabled(false);
   ui_->pushButtonSubmit->setEnabled(false);
   ui_->pushButtonDefault->setEnabled(false);
}
