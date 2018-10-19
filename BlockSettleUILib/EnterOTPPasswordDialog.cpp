#include "EnterOTPPasswordDialog.h"
#include "ui_EnterOTPPasswordDialog.h"
#include <spdlog/spdlog.h>
#include "OTPManager.h"


EnterOTPPasswordDialog::EnterOTPPasswordDialog(const std::shared_ptr<OTPManager> &otpMgr
   , const QString& reason, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::EnterOTPPasswordDialog())
   , auth_(spdlog::get(""))
   , authTimer_(nullptr)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterOTPPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterOTPPasswordDialog::accept);

   ui_->pushButtonOk->setEnabled(false);

   ui_->labelReason->setText(reason);

   if (otpMgr->GetEncType() == bs::wallet::EncryptionType::Auth) {
      ui_->lineEditPassword->hide();
      ui_->labelAuth->show();
      ui_->authTimer->show();
      ui_->authTimer->setFormat(tr("%n second(s) remaining", "", 120));
      authTimer_ = new QTimer(this);
      connect(authTimer_, &QTimer::timeout, this, &EnterOTPPasswordDialog::authTimer);
      connect(&auth_, &AuthSignOTP::succeeded, this, &EnterOTPPasswordDialog::onAuthSucceeded);
      connect(&auth_, &AuthSign::failed, this, &EnterOTPPasswordDialog::onAuthFailed);
      connect(&auth_, &AuthSign::statusUpdated, this, &EnterOTPPasswordDialog::onAuthStatusUpdated);
      auth_.start(otpMgr->GetEncKey(), reason, otpMgr->GetShortId());
      authTimer_->start(1000);
   }
   else {
      ui_->labelAuth->hide();
      ui_->authTimer->hide();
      ui_->lineEditPassword->show();
      connect(ui_->lineEditPassword, &QLineEdit::textEdited, this, &EnterOTPPasswordDialog::passwordChanged);
   }
}

EnterOTPPasswordDialog::~EnterOTPPasswordDialog() = default;

void EnterOTPPasswordDialog::passwordChanged()
{
   ui_->pushButtonOk->setEnabled(!ui_->lineEditPassword->text().isEmpty());
   password_ = ui_->lineEditPassword->text().toStdString();
}

void EnterOTPPasswordDialog::reject()
{
   auth_.stop(true);
   QDialog::reject();
}

void EnterOTPPasswordDialog::onAuthSucceeded(SecureBinaryData password)
{
   password_ = password;
   ui_->pushButtonOk->setEnabled(true);
   accept();
}

void EnterOTPPasswordDialog::onAuthFailed(const QString &)
{
   QDialog::reject();
}

void EnterOTPPasswordDialog::onAuthStatusUpdated(const QString &status)
{
   ui_->labelAuth->setText(tr("Auth status: %1").arg(status));
}

void EnterOTPPasswordDialog::authTimer()
{
   ui_->authTimer->setValue(ui_->authTimer->value() - 1);
   ui_->authTimer->setFormat(tr("%n second(s) remaining", "", ui_->authTimer->value()));

   if (!ui_->authTimer->value()) {
      reject();
   }
}
