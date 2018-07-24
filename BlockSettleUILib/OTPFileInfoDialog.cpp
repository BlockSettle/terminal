#include "OTPFileInfoDialog.h"
#include "ui_OTPFileInfoDialog.h"
#include <spdlog/spdlog.h>
#include "EncryptionUtils.h"
#include "EnterOTPPasswordDialog.h"
#include "MessageBoxCritical.h"
#include "MessageBoxQuestion.h"
#include "OTPFile.h"
#include "OTPManager.h"


OTPFileInfoDialog::OTPFileInfoDialog(const std::shared_ptr<OTPManager>& otpManager
 , QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::OTPFileInfoDialog())
  , otpManager_(otpManager)
  , frejaOld_(spdlog::get(""))
  , frejaNew_(spdlog::get(""))
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &OTPFileInfoDialog::accept);
   connect(ui_->pushButtonRemove, &QPushButton::clicked, this, &OTPFileInfoDialog::RemoveOTP);
   connect(ui_->pushButtonChangePassword, &QPushButton::clicked, this, &OTPFileInfoDialog::onChangePwdClicked);

   connect(ui_->lineEditPwdOld, &QLineEdit::textEdited, this, &OTPFileInfoDialog::onPasswordChanged);
   connect(ui_->lineEditPwdOld, &QLineEdit::editingFinished, this, &OTPFileInfoDialog::onPasswordChanged);
   connect(ui_->lineEditPwdNew1, &QLineEdit::textEdited, this, &OTPFileInfoDialog::onPasswordChanged);
   connect(ui_->lineEditPwdNew1, &QLineEdit::editingFinished, this, &OTPFileInfoDialog::onPasswordChanged);
   connect(ui_->lineEditPwdNew2, &QLineEdit::textEdited, this, &OTPFileInfoDialog::onPasswordChanged);
   connect(ui_->lineEditPwdNew2, &QLineEdit::editingFinished, this, &OTPFileInfoDialog::onPasswordChanged);

   connect(ui_->radioButtonPassword, &QRadioButton::clicked, this, &OTPFileInfoDialog::onEncTypeClicked);
   connect(ui_->radioButtonFreja, &QRadioButton::clicked, this, &OTPFileInfoDialog::onEncTypeClicked);
   connect(ui_->lineEditFrejaId, &QLineEdit::textEdited, this, &OTPFileInfoDialog::onFrejaIdChanged);
   connect(ui_->pushButtonFreja, &QPushButton::clicked, this, &OTPFileInfoDialog::onFrejaClicked);

   connect(&frejaOld_, &FrejaSignOTP::succeeded, this, &OTPFileInfoDialog::onFrejaOldSucceeded);
   connect(&frejaOld_, &FrejaSign::failed, this, &OTPFileInfoDialog::onFrejaOldFailed);
   connect(&frejaOld_, &FrejaSign::statusUpdated, this, &OTPFileInfoDialog::onFrejaOldStatusUpdated);
   connect(&frejaNew_, &FrejaSignOTP::succeeded, this, &OTPFileInfoDialog::onFrejaNewSucceeded);
   connect(&frejaNew_, &FrejaSign::failed, this, &OTPFileInfoDialog::onFrejaNewFailed);
   connect(&frejaNew_, &FrejaSign::statusUpdated, this, &OTPFileInfoDialog::onFrejaNewStatusUpdated);

   ui_->labelWarning->setVisible(false);

   if (otpManager_->CountAdvancingRequired()) {
      if (!UpdateOTPCounter()) {
         ui_->labelWarning->setVisible(true);
         ui_->labelWarning->setText(tr("OTP counter is outdated."));
      }
   }

   ui_->labelOtpId->setText(otpManager_->GetShortId());
   ui_->labelImportDate->setText(otpManager_->GetImportDateString());
   ui_->labelUesdKeys->setText(QString::number(otpManager_->GetUsedKeysCount()));

   QString encType;
   switch (otpManager_->GetEncType()) {
   case bs::wallet::EncryptionType::Unencrypted:
      encType = tr("No");
      break;
   case bs::wallet::EncryptionType::Password:
      encType = tr("Password");
      break;
   case bs::wallet::EncryptionType::Freja:
      encType = tr("Freja eID");
      break;
   default:
      encType = tr("Unknown");
      break;
   }
   ui_->labelEncrypted->setText(encType);

   ui_->groupBoxPassword->hide();
   ui_->widgetFreja->hide();
   ui_->pushButtonFreja->setEnabled(false);
   ui_->labelPwdOld->setVisible(otpManager_->IsEncrypted());
   ui_->lineEditPwdOld->setVisible(otpManager_->IsEncrypted());

}

bool OTPFileInfoDialog::UpdateOTPCounter()
{
   //get password
   EnterOTPPasswordDialog passwordDialog(otpManager_, tr("Enter password to update usage counter"), parentWidget());
   if (passwordDialog.exec() == QDialog::Accepted) {
      const auto &otpPassword = passwordDialog.GetPassword();
      return otpManager_->AdvanceOTPKey(otpPassword);
   }
   return false;
}

void OTPFileInfoDialog::RemoveOTP()
{
   MessageBoxQuestion confirmBox(tr("Remove OTP")
      , tr("Are you sure to remove imported OTP file?")
      , tr("If removed you will not be able to submit request to BlockSettle")
      , this);
   if (confirmBox.exec() == QDialog::Accepted) {
      otpManager_->RemoveOTPForCurrentUser();
      accept();
   }
}

void OTPFileInfoDialog::onPasswordChanged()
{
   if (!ui_->pushButtonChangePassword->isChecked()) {
      ui_->pushButtonOk->setEnabled(true);
      return;
   }
   const auto old = ui_->lineEditPwdOld->text().toStdString();
   oldPassword_ = old;
   const auto new1 = ui_->lineEditPwdNew1->text().toStdString();
   newPassword_ = new1;

   if (otpManager_->GetEncType() == bs::wallet::EncryptionType::Password) {
      if (otpManager_->IsEncrypted()) {
         if (old.empty()) {
            ui_->labelPwdHint->setText(tr("Enter old password"));
            ui_->pushButtonOk->setEnabled(false);
            return;
         }
         else if (!otpManager_->IsPasswordCorrect(old)) {
            ui_->labelPwdHint->setText(tr("Old password is wrong"));
            ui_->pushButtonOk->setEnabled(false);
            return;
         }
      }
   }
   if (ui_->radioButtonPassword->isChecked()) {
      const auto new2 = ui_->lineEditPwdNew2->text().toStdString();
      if (new1.empty() || new2.empty()) {
         ui_->labelPwdHint->setText(tr("Enter new password"));
      }
      else if (new1 != new2) {
         ui_->labelPwdHint->setText(tr("New passwords don't match"));
      }
      else if (!old.empty() && (old == new1)) {
         ui_->labelPwdHint->setText(tr("Old and new passwords shouldn't match"));
      }
      else {
         ui_->labelPwdHint->clear();
         ui_->pushButtonOk->setEnabled(true);
         return;
      }
   }
   ui_->pushButtonOk->setEnabled(false);
}

void OTPFileInfoDialog::onChangePwdClicked()
{
   if (ui_->pushButtonChangePassword->isChecked()) {
      ui_->groupBoxPassword->setVisible(true);

      switch (otpManager_->GetEncType()) {
      case bs::wallet::EncryptionType::Password:
         ui_->widgetPasswordOld->show();
         ui_->labelFrejaOld->hide();
         ui_->lineEditPwdOld->setFocus();
         ui_->labelPwdHint->setText(tr("Enter old password"));
         break;

      case bs::wallet::EncryptionType::Freja:
         ui_->widgetPasswordOld->hide();
         ui_->labelFrejaOld->show();
         ui_->labelPwdHint->setText(tr("Sign with Freja eID"));
         frejaOld_.start(otpManager_->GetEncKey(), tr("Activate Freja eID Signing")
            , tr("One-Time Password (OTP)"));
         break;

      case bs::wallet::EncryptionType::Unencrypted:
      default:
         ui_->widgetPasswordOld->hide();
         ui_->labelFrejaOld->hide();
         ui_->lineEditPwdNew1->setFocus();
         break;
      }
   }
   else {
      ui_->groupBoxPassword->setVisible(false);
      frejaOld_.stop(true);
      frejaNew_.stop(true);
   }
}

void OTPFileInfoDialog::onEncTypeClicked()
{
   ui_->widgetPasswordNew1->setVisible(ui_->radioButtonPassword->isChecked());
   ui_->widgetPasswordNew2->setVisible(ui_->radioButtonPassword->isChecked());
   ui_->widgetFreja->setVisible(ui_->radioButtonFreja->isChecked());
}

void OTPFileInfoDialog::onFrejaIdChanged()
{
   ui_->pushButtonFreja->setEnabled(!ui_->lineEditFrejaId->text().isEmpty());
}

void OTPFileInfoDialog::onFrejaClicked()
{
   ui_->pushButtonFreja->setEnabled(false);
   frejaNew_.start(ui_->lineEditFrejaId->text(), tr("New OTP password")
      , otpManager_->GetShortId());
}

void OTPFileInfoDialog::onFrejaOldSucceeded(SecureBinaryData password)
{
   oldPassword_ = password;
   ui_->labelFrejaOld->setText(tr("Freja signed ok"));
   ui_->labelPwdHint->setText(tr("Enter new encryption credentials"));
}

void OTPFileInfoDialog::onFrejaOldFailed(const QString &text)
{
   ui_->pushButtonChangePassword->setChecked(false);
   onChangePwdClicked();
}

void OTPFileInfoDialog::onFrejaOldStatusUpdated(const QString &status)
{
   ui_->labelFrejaOld->setText(tr("Freja sign status: %1").arg(status));
}

void OTPFileInfoDialog::onFrejaNewSucceeded(SecureBinaryData password)
{
   newPassword_ = password;
   ui_->pushButtonFreja->setText(tr("Freja signed ok"));
   ui_->pushButtonOk->setEnabled(true);
}

void OTPFileInfoDialog::onFrejaNewFailed(const QString &text)
{
   reject();
}

void OTPFileInfoDialog::onFrejaNewStatusUpdated(const QString &status)
{
   ui_->pushButtonFreja->setText(status);
}

void OTPFileInfoDialog::reject()
{
   frejaOld_.stop(true);
   frejaNew_.stop(true);
   QDialog::reject();
}

void OTPFileInfoDialog::accept()
{
   if (!ui_->pushButtonChangePassword->isChecked()) {
      QDialog::accept();
      return;
   }

   auto cb = [this] (const std::shared_ptr<OTPFile> &otp, SecureBinaryData &oldPwd, SecureBinaryData &newPwd
      , bs::wallet::EncryptionType &encType, SecureBinaryData &encKey) -> bool {
      oldPwd = oldPassword_;
      if (otp->encryptionType() == bs::wallet::EncryptionType::Password) {
         if (ui_->lineEditPwdOld->text().isEmpty()) {
            return false;
         }
      }
      newPwd = newPassword_;
      if (ui_->radioButtonPassword->isChecked()) {
         encType = bs::wallet::EncryptionType::Password;
      }
      else if (ui_->radioButtonFreja->isChecked()) {
         encType = bs::wallet::EncryptionType::Freja;
         encKey = ui_->lineEditFrejaId->text().toStdString();
      }
      return true;
   };

   if (!otpManager_->UpdatePassword(cb)) {
      MessageBoxCritical(tr("OTP password failed"), tr("Failed to set or change OTP password")).exec();
      QDialog::reject();
   }
   QDialog::accept();
}
