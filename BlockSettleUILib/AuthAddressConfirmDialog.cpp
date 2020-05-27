/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AuthAddressConfirmDialog.h"

#include "AuthAddressConfirmDialog.h"
#include "BsClient.h"
#include "BSMessageBox.h"
#include "ApplicationSettings.h"
#include "ui_AuthAddressConfirmDialog.h"

namespace {
   constexpr auto UiTimerInterval = std::chrono::milliseconds(250);
}

AuthAddressConfirmDialog::AuthAddressConfirmDialog(const std::weak_ptr<BsClient> &bsClient, const bs::Address& address
   , const std::shared_ptr<AuthAddressManager>& authManager, const std::shared_ptr<ApplicationSettings> &settings, QWidget* parent)
   : QDialog(parent)
   , ui_{new Ui::AuthAddressConfirmDialog()}
   , address_{address}
   , authManager_{authManager}
   , settings_(settings)
   , bsClient_(bsClient)
{
   ui_->setupUi(this);

   setWindowTitle(tr("Auth eID Signing Request"));
   ui_->labelDescription->setText(QString::fromStdString(address.display()));

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &AuthAddressConfirmDialog::onCancelPressed);

   // setup timer
   progressTimer_.setSingleShot(false);
   progressTimer_.setInterval(UiTimerInterval);

   ui_->progressBarTimeout->setMinimum(0);
   const int timeoutMs = int(std::chrono::duration_cast<std::chrono::milliseconds>(BsClient::autheidAuthAddressTimeout()).count());
   ui_->progressBarTimeout->setMaximum(timeoutMs);
   ui_->progressBarTimeout->setValue(ui_->progressBarTimeout->maximum());
   ui_->progressBarTimeout->setFormat(QString());

   connect(&progressTimer_, &QTimer::timeout, this, &AuthAddressConfirmDialog::onUiTimerTick);

   // connect to auth manager
   connect(authManager_.get(), &AuthAddressManager::AuthAddrSubmitError, this, &AuthAddressConfirmDialog::onAuthAddrSubmitError, Qt::QueuedConnection);
   connect(authManager_.get(), &AuthAddressManager::AuthConfirmSubmitError, this, &AuthAddressConfirmDialog::onAuthConfirmSubmitError, Qt::QueuedConnection);
   connect(authManager_.get(), &AuthAddressManager::AuthAddrSubmitSuccess, this, &AuthAddressConfirmDialog::onAuthAddrSubmitSuccess, Qt::QueuedConnection);
   connect(authManager_.get(), &AuthAddressManager::AuthAddressSubmitCancelled, this, &AuthAddressConfirmDialog::onAuthAddressSubmitCancelled, Qt::QueuedConnection);

   // send confirm request
   startTime_ = std::chrono::steady_clock::now();
   authManager_->ConfirmSubmitForVerification(bsClient, address);
   progressTimer_.start();
}

AuthAddressConfirmDialog::~AuthAddressConfirmDialog() = default;

void AuthAddressConfirmDialog::onUiTimerTick()
{
   // Do not check timeout here, BsClient will detect it itself

   const auto timeLeft = BsClient::autheidAuthAddressTimeout() - (std::chrono::steady_clock::now() - startTime_);

   const int countMs = std::max(0, int(std::chrono::duration_cast<std::chrono::milliseconds>(timeLeft).count()));
   ui_->progressBarTimeout->setValue(countMs);
   ui_->labelTimeLeft->setText(tr("%1 seconds left").arg(countMs / 1000));
}

void AuthAddressConfirmDialog::onCancelPressed()
{
   reject();
}

void AuthAddressConfirmDialog::reject()
{
   progressTimer_.stop();

   auto bsClient = bsClient_.lock();
   if (bsClient) {
      bsClient->cancelActiveSign();
   }

   QDialog::reject();
}

void AuthAddressConfirmDialog::onAuthAddressSubmitCancelled(const QString &address)
{
   reject();
}

void AuthAddressConfirmDialog::onError(const QString &errorText)
{
   // error message should be displayed by parent
   hide();
   parentWidget()->setFocus();
   reject();
}

void AuthAddressConfirmDialog::onAuthAddrSubmitError(const QString &address, const QString &error)
{
   progressTimer_.stop();
   BSMessageBox(BSMessageBox::critical, tr("Submission")
      , tr("Submission failed")
      , error, this).exec();
   reject();
}

void AuthAddressConfirmDialog::onAuthConfirmSubmitError(const QString &address, const QString &error)
{
   progressTimer_.stop();
   BSMessageBox(BSMessageBox::critical, tr("Confirmation")
      , tr("Confirmation failed")
      , error, this).exec();
   reject();
}

void AuthAddressConfirmDialog::onAuthAddrSubmitSuccess(const QString &address)
{
   progressTimer_.stop();

   const bool isProd = settings_->get<int>(ApplicationSettings::envConfiguration) ==
      static_cast<int>(ApplicationSettings::EnvConfiguration::Production);

   const auto body = isProd ? tr("A validation transaction will be sent within the next 24 hours.")
      : tr("Within the next 15 minutes, BlockSettle initiates the validation transaction.\n\n"
         "Once mined six blocks, you have access to bitcoin trading.\n");

   BSMessageBox(BSMessageBox::success, tr("Submission Successful")
      , tr("Authentication Address Submitted")
      , body
      , this).exec();
   accept();
}
