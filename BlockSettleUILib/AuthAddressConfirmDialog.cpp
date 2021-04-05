/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
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
   connect(authManager_.get(), &AuthAddressManager::AuthAddressSubmitError, this, &AuthAddressConfirmDialog::onAuthAddressSubmitError, Qt::QueuedConnection);
   connect(authManager_.get(), &AuthAddressManager::AuthAddressSubmitSuccess, this, &AuthAddressConfirmDialog::onAuthAddressSubmitSuccess, Qt::QueuedConnection);
   connect(authManager_.get(), &AuthAddressManager::AuthAddressSubmitCancelled, this, &AuthAddressConfirmDialog::onAuthAddressSubmitCancelled, Qt::QueuedConnection);

   // send confirm request
   startTime_ = std::chrono::steady_clock::now();
   authManager_->ConfirmSubmitForVerification(bsClient, address);
   progressTimer_.start();
}

AuthAddressConfirmDialog::AuthAddressConfirmDialog(const bs::Address& address
   , QWidget* parent)
   : QDialog(parent)
   , ui_{ new Ui::AuthAddressConfirmDialog() }
   , address_{ address }
{
   ui_->setupUi(this);

   setWindowTitle(tr("Confirm auth address submission"));
   ui_->labelDescription->setText(QString::fromStdString(address.display()));

   ui_->pushButtonCancel->setText(tr("Ok"));
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &QDialog::accept);
//   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &AuthAddressConfirmDialog::onCancelPressed);
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

void AuthAddressConfirmDialog::onAuthAddressSubmitError(const QString &address, const bs::error::AuthAddressSubmitResult statusCode)
{
   progressTimer_.stop();

   QString errorText;

   switch (statusCode) {
   case bs::error::AuthAddressSubmitResult::SubmitLimitExceeded:
      errorText = tr("Your account has reached the limit of how many "
                     "Authentication Addresses it may have in submitted state."
                     " Please verify your currently submitted addresses before "
                     "submitting further addresses.");
      break;
   case bs::error::AuthAddressSubmitResult::ServerError:
      errorText = tr("Server error. Please try again later.");
      break;
   case bs::error::AuthAddressSubmitResult::AlreadyUsed:
      errorText = tr("Authentication Address already in use.");
      break;
   case bs::error::AuthAddressSubmitResult::RequestTimeout:
      errorText = tr("Request processing timeout.");
      break;
   case bs::error::AuthAddressSubmitResult::AuthRequestSignFailed:
      errorText = tr("Failed to sign request to submit Authentication Address.");
      break;
   default:
      errorText = tr("Undefined error code.");
      break;
   }

   BSMessageBox(BSMessageBox::critical, tr("Submission")
      , tr("Submission failed")
      , errorText, this).exec();
   reject();
}

void AuthAddressConfirmDialog::onAuthAddressSubmitSuccess(const QString &address)
{
   progressTimer_.stop();

   const bool isProd = settings_->get<int>(ApplicationSettings::envConfiguration) ==
      static_cast<int>(ApplicationSettings::EnvConfiguration::Production);

   BSMessageBox(BSMessageBox::success, tr("Submission Successful")
      , tr("Authentication Address Submitted")
      , tr("You now have access to Spot XBT (bitcoin) trading.")
      , this).exec();
   accept();
}
