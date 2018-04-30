#include "OTPFileInfoDialog.h"
#include "ui_OTPFileInfoDialog.h"

#include "OTPFile.h"

#include "EncryptionUtils.h"
#include "EnterOTPPasswordDialog.h"
#include "MessageBoxCritical.h"
#include "MessageBoxQuestion.h"
#include "OTPManager.h"

OTPFileInfoDialog::OTPFileInfoDialog(const std::shared_ptr<OTPManager>& otpManager
 , QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::OTPFileInfoDialog())
  , otpManager_(otpManager)
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
   ui_->labelEncrypted->setText(otpManager_->IsEncrypted() ? tr("Yes") : tr("No"));

   ui_->groupBoxPassword->hide();
   ui_->labelPwdOld->setVisible(otpManager_->IsEncrypted());
   ui_->lineEditPwdOld->setVisible(otpManager_->IsEncrypted());

}

bool OTPFileInfoDialog::UpdateOTPCounter()
{
   //get password
   EnterOTPPasswordDialog passwordDialog{tr("Enter password to update usage counter"), parentWidget()};
   if (passwordDialog.exec() == QDialog::Accepted) {
      auto otpPassword = SecureBinaryData(passwordDialog.GetPassword().toStdString());

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
   const auto new1 = ui_->lineEditPwdNew1->text().toStdString();
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
   ui_->pushButtonOk->setEnabled(false);
}

void OTPFileInfoDialog::onChangePwdClicked()
{
   if (ui_->pushButtonChangePassword->isChecked()) {
      ui_->groupBoxPassword->setVisible(true);
      if (otpManager_->IsEncrypted()) {
         ui_->lineEditPwdOld->setFocus();
      } else {
         ui_->lineEditPwdNew1->setFocus();
      }
   } else {
      ui_->groupBoxPassword->setVisible(false);
   }

   onPasswordChanged();
}

void OTPFileInfoDialog::accept()
{
   if (!ui_->pushButtonChangePassword->isChecked()) {
      QDialog::accept();
      return;
   }

   auto cb = [this] (SecureBinaryData &oldPwd, SecureBinaryData &newPwd) -> bool {
      std::string old;
      if (otpManager_->IsEncrypted()) {
         old = ui_->lineEditPwdOld->text().toStdString();
         if (old.empty()) {
            return false;
         }
      }
      const auto new1 = ui_->lineEditPwdNew1->text().toStdString();
      const auto new2 = ui_->lineEditPwdNew2->text().toStdString();
      if ((new1 != new2) || new1.empty() || new2.empty() || (!old.empty() && (old == new1))) {
         return false;
      }
      oldPwd = SecureBinaryData(old);
      newPwd = SecureBinaryData(new1);
      return true;
   };

   if (!otpManager_->UpdatePassword(cb)) {
      MessageBoxCritical(tr("OTP password failed"), tr("Failed to set or change OTP password")).exec();
      QDialog::reject();
   }
   QDialog::accept();
}
