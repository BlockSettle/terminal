#include "EnterOTPPasswordDialog.h"
#include "ui_EnterOTPPasswordDialog.h"
#include <spdlog/spdlog.h>
#include "OTPManager.h"


EnterOTPPasswordDialog::EnterOTPPasswordDialog(const std::shared_ptr<OTPManager> &otpMgr
   , const QString& reason, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::EnterOTPPasswordDialog())
   , freja_(spdlog::get(""))
   , frejaTimer_(nullptr)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterOTPPasswordDialog::reject);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterOTPPasswordDialog::accept);

   ui_->pushButtonOk->setEnabled(false);

   ui_->labelReason->setText(reason);

   if (otpMgr->GetEncType() == bs::wallet::EncryptionType::Freja) {
      ui_->lineEditPassword->hide();
      ui_->labelFreja->show();
      ui_->frejaTimer->show();
      ui_->frejaTimer->setFormat(tr("%n second(s) remaining", "", 120));
      frejaTimer_ = new QTimer(this);
      connect(frejaTimer_, &QTimer::timeout, this, &EnterOTPPasswordDialog::frejaTimer);
      connect(&freja_, &FrejaSignOTP::succeeded, this, &EnterOTPPasswordDialog::onFrejaSucceeded);
      connect(&freja_, &FrejaSign::failed, this, &EnterOTPPasswordDialog::onFrejaFailed);
      connect(&freja_, &FrejaSign::statusUpdated, this, &EnterOTPPasswordDialog::onFrejaStatusUpdated);
      freja_.start(otpMgr->GetEncKey(), reason, otpMgr->GetShortId());
      frejaTimer_->start(1000);
   }
   else {
      ui_->labelFreja->hide();
      ui_->frejaTimer->hide();
      ui_->lineEditPassword->show();
      connect(ui_->lineEditPassword, &QLineEdit::textEdited, this, &EnterOTPPasswordDialog::passwordChanged);
   }
}

void EnterOTPPasswordDialog::passwordChanged()
{
   ui_->pushButtonOk->setEnabled(!ui_->lineEditPassword->text().isEmpty());
   password_ = ui_->lineEditPassword->text().toStdString();
}

void EnterOTPPasswordDialog::reject()
{
   freja_.stop(true);
   QDialog::reject();
}

void EnterOTPPasswordDialog::onFrejaSucceeded(SecureBinaryData password)
{
   password_ = password;
   ui_->pushButtonOk->setEnabled(true);
   accept();
}

void EnterOTPPasswordDialog::onFrejaFailed(const QString &)
{
   QDialog::reject();
}

void EnterOTPPasswordDialog::onFrejaStatusUpdated(const QString &status)
{
   ui_->labelFreja->setText(tr("Freja status: %1").arg(status));
}

void EnterOTPPasswordDialog::frejaTimer()
{
   ui_->frejaTimer->setValue(ui_->frejaTimer->value() - 1);
   ui_->frejaTimer->setFormat(tr("%n second(s) remaining", "", ui_->frejaTimer->value()));

   if (!ui_->frejaTimer->value()) {
      reject();
   }
}
