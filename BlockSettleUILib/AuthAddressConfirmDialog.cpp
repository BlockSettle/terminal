#include "AuthAddressConfirmDialog.h"

#include "AuthAddressConfirmDialog.h"
#include "BsClient.h"
#include "BSMessageBox.h"
#include "ui_AuthAddressConfirmDialog.h"

namespace {
constexpr auto UiTimerInterval = std::chrono::milliseconds(250);
}

AuthAddressConfirmDialog::AuthAddressConfirmDialog(BsClient *bsClient, const bs::Address& address
   , const std::shared_ptr<AuthAddressManager>& authManager, QWidget* parent)
  : QDialog(parent)
  , ui_{new Ui::AuthAddressConfirmDialog()}
  , address_{address}
  , authManager_{authManager}
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
   connect(authManager_.get(), &AuthAddressManager::signFailed, this, &AuthAddressConfirmDialog::onSignFailed, Qt::QueuedConnection);

   // send confirm request
   startTime_ = std::chrono::steady_clock::now();
   authManager_->ConfirmSubmitForVerification(bsClient, address);
   progressTimer_.start();
}

AuthAddressConfirmDialog::~AuthAddressConfirmDialog() = default;

void AuthAddressConfirmDialog::onUiTimerTick()
{
   const auto timeLeft = BsClient::autheidAuthAddressTimeout() - (std::chrono::steady_clock::now() - startTime_);
   if (timeLeft.count() < 0) {
      CancelSubmission();
   } else {
      const int countMs = int(std::chrono::duration_cast<std::chrono::milliseconds>(timeLeft).count());
      ui_->progressBarTimeout->setValue(countMs);
      ui_->labelTimeLeft->setText(tr("%1 seconds left").arg(int(countMs / 1000.0)));
   }
}

void AuthAddressConfirmDialog::onCancelPressed()
{
   CancelSubmission();
}

void AuthAddressConfirmDialog::CancelSubmission()
{
   progressTimer_.stop();

   authManager_->CancelSubmitForVerification(address_);
}

void AuthAddressConfirmDialog::onAuthAddressSubmitCancelled(const QString &address)
{
   reject();
}

void AuthAddressConfirmDialog::onError(const QString &errorText)
{
   // error message should be displayed by parent
   reject();
}

void AuthAddressConfirmDialog::onAuthAddrSubmitError(const QString &address, const QString &error)
{
   BSMessageBox(BSMessageBox::critical, tr("Submission Aborted")
      , tr("The process of submitting an Authentication Address has been aborted."
           "Any reserved balance will be returned.")
      , error, this).exec();

   reject();
}

void AuthAddressConfirmDialog::onAuthConfirmSubmitError(const QString &address, const QString &error)
{
   BSMessageBox(BSMessageBox::critical, tr("Confirmation Aborted")
      , tr("The process of submitting an Authentication Address has been aborted."
           "Any reserved balance will be returned.")
      , error, this).exec();

   reject();
}

void AuthAddressConfirmDialog::onAuthAddrSubmitSuccess(const QString &address)
{
   BSMessageBox(BSMessageBox::info, tr("Submission Successful")
      , tr("Your Authentication Address has now been submitted.")
      , tr("Please allow BlockSettle 24 hours to fund your Authentication Address.")
      , this).exec();

   accept();
}

void AuthAddressConfirmDialog::onSignFailed(AutheIDClient::ErrorType error)
{
   // explicitly stop timer before cancel on submission, user need time to read MessageBox text
   progressTimer_.stop();

   BSMessageBox(BSMessageBox::critical, tr("Request sign failed")
      , tr("The process of submitting an Authentication Address has been aborted."
           "Any reserved balance will be returned.")
      , this).exec();

   CancelSubmission();
}
