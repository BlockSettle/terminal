#include "OTPFileInfoDialog.h"
#include "ui_OTPFileInfoDialog.h"

#include <spdlog/spdlog.h>
#include "EncryptionUtils.h"
#include "EnterOTPPasswordDialog.h"
#include "BSMessageBox.h"
#include "OTPFile.h"
#include "OTPManager.h"


OTPFileInfoDialog::OTPFileInfoDialog(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<OTPManager>& otpManager
   , const std::shared_ptr<ApplicationSettings> &settings
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::OTPFileInfoDialog())
   , logger_(logger)
   , otpManager_(otpManager)
   , settings_(settings)
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
   connect(ui_->radioButtonAuth, &QRadioButton::clicked, this, &OTPFileInfoDialog::onEncTypeClicked);
   connect(ui_->lineEditAuthId, &QLineEdit::textEdited, this, &OTPFileInfoDialog::onAuthIdChanged);
   connect(ui_->pushButtonAuth, &QPushButton::clicked, this, &OTPFileInfoDialog::onAuthClicked);

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
   case bs::wallet::EncryptionType::Auth:
      encType = tr("Auth eID");
      break;
   default:
      encType = tr("Unknown");
      break;
   }
   ui_->labelEncrypted->setText(encType);

   ui_->groupBoxPassword->hide();
   ui_->widgetAuth->hide();
   ui_->pushButtonAuth->setEnabled(false);
   ui_->labelPwdOld->setVisible(otpManager_->IsEncrypted());
   ui_->lineEditPwdOld->setVisible(otpManager_->IsEncrypted());

}

OTPFileInfoDialog::~OTPFileInfoDialog() = default;

bool OTPFileInfoDialog::UpdateOTPCounter()
{
   //get password
   EnterOTPPasswordDialog passwordDialog(logger_, otpManager_
      , tr("Enter password to update usage counter"), settings_, parentWidget());
   if (passwordDialog.exec() == QDialog::Accepted) {
      const auto &otpPassword = passwordDialog.GetPassword();
      return otpManager_->AdvanceOTPKey(otpPassword);
   }
   return false;
}

void OTPFileInfoDialog::RemoveOTP()
{
   BSMessageBox confirmBox(BSMessageBox::question, tr("Remove OTP")
      , tr("Are you sure to remove imported OTP file?")
      , tr("If removed you will not be able to submit a request to BlockSettle")
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
         ui_->labelAuthOld->hide();
         ui_->lineEditPwdOld->setFocus();
         ui_->labelPwdHint->setText(tr("Enter old password"));
         break;

      case bs::wallet::EncryptionType::Auth:
         ui_->widgetPasswordOld->hide();
         ui_->labelAuthOld->show();
         ui_->labelPwdHint->setText(tr("Sign with Auth eID"));
         break;

      case bs::wallet::EncryptionType::Unencrypted:
      default:
         ui_->widgetPasswordOld->hide();
         ui_->labelAuthOld->hide();
         ui_->lineEditPwdNew1->setFocus();
         break;
      }
   }
   else {
      ui_->groupBoxPassword->setVisible(false);
   }
}

void OTPFileInfoDialog::onEncTypeClicked()
{
   ui_->widgetPasswordNew1->setVisible(ui_->radioButtonPassword->isChecked());
   ui_->widgetPasswordNew2->setVisible(ui_->radioButtonPassword->isChecked());
   ui_->widgetAuth->setVisible(ui_->radioButtonAuth->isChecked());
}

void OTPFileInfoDialog::onAuthIdChanged()
{
   ui_->pushButtonAuth->setEnabled(!ui_->lineEditAuthId->text().isEmpty());
}

void OTPFileInfoDialog::onAuthClicked()
{
   ui_->pushButtonAuth->setEnabled(false);
}

void OTPFileInfoDialog::onAuthOldSucceeded(SecureBinaryData password)
{
   oldPassword_ = password;
   ui_->labelAuthOld->setText(tr("Auth signed ok"));
   ui_->labelPwdHint->setText(tr("Enter new encryption credentials"));
}

void OTPFileInfoDialog::onAuthOldFailed(const QString &text)
{
   ui_->pushButtonChangePassword->setChecked(false);
   onChangePwdClicked();
}

void OTPFileInfoDialog::onAuthOldStatusUpdated(const QString &status)
{
   ui_->labelAuthOld->setText(tr("Auth sign status: %1").arg(status));
}

void OTPFileInfoDialog::onAuthNewSucceeded(SecureBinaryData password)
{
   newPassword_ = password;
   ui_->pushButtonAuth->setText(tr("Auth signed ok"));
   ui_->pushButtonOk->setEnabled(true);
}

void OTPFileInfoDialog::onAuthNewFailed(const QString &text)
{
   reject();
}

void OTPFileInfoDialog::onAuthNewStatusUpdated(const QString &status)
{
   ui_->pushButtonAuth->setText(status);
}

void OTPFileInfoDialog::reject()
{
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
      else if (ui_->radioButtonAuth->isChecked()) {
         encType = bs::wallet::EncryptionType::Auth;
         encKey = ui_->lineEditAuthId->text().toStdString();
      }
      return true;
   };

   if (!otpManager_->UpdatePassword(cb)) {
      BSMessageBox(BSMessageBox::critical, tr("OTP password failed"), 
         tr("Failed to set or change OTP password")).exec();
      QDialog::reject();
   }
   QDialog::accept();
}
