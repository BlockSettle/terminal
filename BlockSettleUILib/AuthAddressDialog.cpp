/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AuthAddressDialog.h"
#include "ui_AuthAddressDialog.h"

#include <spdlog/spdlog.h>
#include <QItemSelection>
#include <QMouseEvent>
#include <QMenu>
#include <QClipboard>
#include <QEvent>

#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "AuthAddressConfirmDialog.h"
#include "AuthAddressManager.h"
#include "AuthAddressViewModel.h"
#include "BSMessageBox.h"
#include "UiUtils.h"


AuthAddressDialog::AuthAddressDialog(const std::shared_ptr<spdlog::logger>& logger
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::AuthAddressDialog())
   , logger_(logger)
{
   ui_->setupUi(this);

   authModel_ = new AuthAddressViewModel(ui_->treeViewAuthAdress);
   model_ = new AuthAdressControlProxyModel(authModel_, this);
   model_->setVisibleRowsCount(10);  //FIXME
//   model_->setVisibleRowsCount(settings_->get<int>(ApplicationSettings::numberOfAuthAddressVisible));
   ui_->treeViewAuthAdress->setModel(model_);
   ui_->treeViewAuthAdress->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewAuthAdress->installEventFilter(this);

   connect(ui_->treeViewAuthAdress->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &AuthAddressDialog::adressSelected);
   connect(model_, &AuthAdressControlProxyModel::modelReset, this, &AuthAddressDialog::onModelReset);
   connect(authModel_, &AuthAddressViewModel::updateSelectionAfterReset, this, &AuthAddressDialog::onUpdateSelection);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &AuthAddressDialog::createAddress);
   connect(ui_->pushButtonRevoke, &QPushButton::clicked, this, &AuthAddressDialog::revokeSelectedAddress);
   connect(ui_->pushButtonSubmit, &QPushButton::clicked, this, &AuthAddressDialog::submitSelectedAddress);
   connect(ui_->pushButtonDefault, &QPushButton::clicked, this, &AuthAddressDialog::setDefaultAddress);
}

AuthAddressDialog::~AuthAddressDialog() = default;

bool AuthAddressDialog::eventFilter(QObject* sender, QEvent* event)
{
   if (sender == ui_->treeViewAuthAdress) {
      if (QEvent::KeyPress == event->type()) {
         QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

         if (keyEvent->matches(QKeySequence::Copy)) {
            copySelectedToClipboard();
            return true;
         }
      }
      else if (QEvent::ContextMenu == event->type()) {
         QContextMenuEvent* contextMenuEvent = static_cast<QContextMenuEvent*>(event);

         QPoint pos = contextMenuEvent->pos();
         pos.setY(pos.y() - ui_->treeViewAuthAdress->header()->height());
         const auto index = ui_->treeViewAuthAdress->indexAt(pos);
         if (index.isValid()) {
            if (ui_->treeViewAuthAdress->selectionModel()->selectedIndexes()[0] != index) {
               ui_->treeViewAuthAdress->selectionModel()->select(index,
                  QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }

            QMenu menu;
            menu.addAction(tr("&Copy Authentication Address"), [this]() {
               copySelectedToClipboard();
            });
            menu.exec(contextMenuEvent->globalPos());
            return true;
         }
      }
   }

   return QWidget::eventFilter(sender, event);
}

void AuthAddressDialog::showEvent(QShowEvent *evt)
{
   if (defaultAddr_.empty()) {
      authModel_->setDefaultAddr(defaultAddr_);
   }

   updateUnsubmittedState();

   ui_->treeViewAuthAdress->selectionModel()->clearSelection();

   QModelIndex index = model_->getFirstUnsubmitted();
   if (!index.isValid() && !model_->isEmpty()) {
      // get first if none unsubmitted
      index = model_->index(0, 0);
   }

   if (index.isValid()) {
      ui_->treeViewAuthAdress->selectionModel()->select(
         index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
   }

   ui_->labelHint->clear();

   resizeTreeViewColumns();

   QDialog::showEvent(evt);
}

bool AuthAddressDialog::unsubmittedExist() const
{
   return unconfirmedExists_;
}

void AuthAddressDialog::updateUnsubmittedState()
{
#if 0
   if (authAddressManager_) {
      for (size_t i = 0; i < authAddressManager_->GetAddressCount(); i++) {
         const auto authAddr = authAddressManager_->GetAddress(i);
         if (!authAddressManager_->hasSettlementLeaf(authAddr)) {
            authAddressManager_->createSettlementLeaf(authAddr, [] {});
            unconfirmedExists_ = true;
            return;
         }
         switch (authAddressManager_->GetState(authAddr)) {
         case AuthAddressManager::AuthAddressState::Verifying:
            unconfirmedExists_ = true;
            break;
         default:
            unconfirmedExists_ = false;
            break;
         }
      }
   }
#endif   //0
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

void AuthAddressDialog::onAuthAddresses(const std::vector<bs::Address>& addrs
   , const std::map<bs::Address, AddressVerificationState>& states)
{
   authModel_->onAuthAddresses(addrs, states);
}

void AuthAddressDialog::onAddrWhitelisted(const std::map<bs::Address, AddressVerificationState>& addrs)
{
   authModel_->onAddrWhitelisted(addrs);
}

void AuthAddressDialog::onSubmittedAuthAddresses(const std::vector<bs::Address>& addrs)
{
   authModel_->onSubmittedAuthAddresses(addrs);
}

void AuthAddressDialog::onAuthVerifyTxSent()
{
   BSMessageBox(BSMessageBox::info, tr("Authentication Address")
      , tr("Verification Transaction successfully sent.")
      , tr("Once the validation transaction is mined six (6) blocks deep, your Authentication Address will be"
         " accepted as valid by the network and you can enter orders in the Spot XBT product group.")).exec();
   accept();
}

void AuthAddressDialog::onUpdateSelection(int row)
{
   if (row < 0 || row >= model_->rowCount()) {
      return;
   }

   ui_->treeViewAuthAdress->selectionModel()->select(
      model_->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void AuthAddressDialog::copySelectedToClipboard()
{
   auto *selectionModel = ui_->treeViewAuthAdress->selectionModel();
   if (!selectionModel->hasSelection()) {
      return;
   }

   auto const address = model_->getAddress(selectionModel->selectedIndexes()[0]);
   qApp->clipboard()->setText(QString::fromStdString(address.display()));
}


void AuthAddressDialog::onAddrVerifiedOrRevoked(const QString &addr, int st)
{
   const auto state = static_cast<AuthCallbackTarget::AuthAddressState>(st);
   if (state == AuthCallbackTarget::AuthAddressState::Verified) {
      BSMessageBox(BSMessageBox::success, tr("Authentication Address")
         , tr("Authentication Address verified")
         , tr("You may now place orders in the Spot XBT product group.")
         ).exec();
   } else if (state == AuthCallbackTarget::AuthAddressState::Revoked) {
      BSMessageBox(BSMessageBox::warning, tr("Authentication Address")
         , tr("Authentication Address revoked")
         , tr("Authentication Address %1 was revoked and could not be used for Spot XBT trading.").arg(addr)).exec();
   }
   updateEnabledStates();
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

void AuthAddressDialog::adressSelected()
{
   updateEnabledStates();
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
/*      if (nbAddresses > model_->getVisibleRowsCount()) {
         model_->increaseVisibleRowsCountByOne();
         saveAddressesNumber();
         onModelReset();
         return;
      }*/
   emit needNewAuthAddress();
   ui_->pushButtonCreate->setEnabled(false);
   model_->increaseVisibleRowsCountByOne();
   saveAddressesNumber();
}

void AuthAddressDialog::revokeSelectedAddress()
{
   auto selectedAddress = GetSelectedAddress();
   if (selectedAddress.empty()) {
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
      //TODO: reimplement if needed: authAddressManager_->RevokeAddress(selectedAddress);
   }
}

void AuthAddressDialog::submitSelectedAddress()
{
   ui_->pushButtonSubmit->setEnabled(false);
   ui_->labelHint->clear();

   const auto selectedAddress = GetSelectedAddress();
   if (selectedAddress.empty()) {
      return;
   }

   auto confirmDlg = new AuthAddressConfirmDialog(selectedAddress, this);
   if (confirmDlg->exec() == QDialog::Accepted) {
      emit needSubmitAuthAddress(selectedAddress);
   }
}

void AuthAddressDialog::setDefaultAddress()
{
   auto selectedAddress = GetSelectedAddress();
   if (!selectedAddress.empty()) {
      defaultAddr_ = selectedAddress;
      authModel_->setDefaultAddr(defaultAddr_);
      emit setDefaultAuthAddress(defaultAddr_);
      resizeTreeViewColumns();
   }
}

void AuthAddressDialog::onModelReset()
{
   updateEnabledStates();
   saveAddressesNumber();
   adressSelected();
}

void AuthAddressDialog::saveAddressesNumber()
{
   const int newNumber = std::max(1, model_->rowCount());
   const int oldNumber = 10;   //FIXME: read actual value from settings
   if (newNumber == oldNumber) {
      return; // nothing to save
   }
   else if (newNumber > oldNumber) {
      // we have added address
      emit onUpdateSelection(model_->rowCount() - 1);
   }

   emit putSetting(ApplicationSettings::numberOfAuthAddressVisible, newNumber);

   if (model_->isEmpty()) {
      model_->setVisibleRowsCount(1);
   }
}

void AuthAddressDialog::setLastSubmittedAddress(const bs::Address &address)
{
   if (lastSubmittedAddress_ != address) {
      lastSubmittedAddress_ = address;
      updateEnabledStates();
   }
}

void AuthAddressDialog::updateEnabledStates()
{
   const auto selectionModel = ui_->treeViewAuthAdress->selectionModel();
   if (selectionModel->hasSelection()) {
      const auto address = model_->getAddress(selectionModel->selectedRows()[0]);

      switch (authModel_->getState(address)) {
      case AddressVerificationState::Verified:
         ui_->pushButtonRevoke->setEnabled(true);
         ui_->pushButtonSubmit->setEnabled(false);
         ui_->pushButtonDefault->setEnabled(address != defaultAddr_);
         break;
      default:
         ui_->pushButtonRevoke->setEnabled(false);
         ui_->pushButtonSubmit->setEnabled(authModel_->canSubmit(address));
         ui_->pushButtonDefault->setEnabled(false);
         break;
      }
   }
   else {
      ui_->pushButtonRevoke->setEnabled(false);
      ui_->pushButtonSubmit->setEnabled(false);
      ui_->pushButtonDefault->setEnabled(false);
   }

   ui_->pushButtonCreate->setEnabled(lastSubmittedAddress_.empty() &&
      model_ && !model_->isUnsubmittedAddressVisible());
}
