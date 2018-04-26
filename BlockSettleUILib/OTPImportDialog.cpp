#include "OTPImportDialog.h"
#include "ui_OTPImportDialog.h"

#include "EasyCoDec.h"
#include "EasyEncValidator.h"
#include "EncryptionUtils.h"
#include "MessageBoxCritical.h"
#include "OTPFile.h"
#include "OTPManager.h"

OTPImportDialog::OTPImportDialog(const std::shared_ptr<OTPManager>& otpManager, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::OTPImportDialog())
   , otpManager_(otpManager)
   , easyCodec_(std::make_shared<EasyCoDec>())
{
   ui_->setupUi(this);

   ui_->pushButtonOk->setEnabled(false);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &OTPImportDialog::accept);

   connect(ui_->lineEditPwd1, &QLineEdit::textEdited, this, &OTPImportDialog::onPasswordChanged);
   connect(ui_->lineEditPwd1, &QLineEdit::editingFinished, this, &OTPImportDialog::onPasswordChanged);
   connect(ui_->lineEditPwd2, &QLineEdit::textEdited, this, &OTPImportDialog::onPasswordChanged);
   connect(ui_->lineEditPwd2, &QLineEdit::editingFinished, this, &OTPImportDialog::onPasswordChanged);

   validator_ = new EasyEncValidator(easyCodec_);
   ui_->lineEditOtp1->setValidator(validator_);
   connect(ui_->lineEditOtp1, &QLineEdit::textEdited, this, &OTPImportDialog::keyTextChanged);
   connect(ui_->lineEditOtp1, &QLineEdit::editingFinished, this, &OTPImportDialog::keyTextChanged);
   ui_->lineEditOtp2->setValidator(validator_);
   connect(ui_->lineEditOtp2, &QLineEdit::textEdited, this, &OTPImportDialog::keyTextChanged);
   connect(ui_->lineEditOtp2, &QLineEdit::editingFinished, this, &OTPImportDialog::keyTextChanged);
}

OTPImportDialog::~OTPImportDialog()
{
   delete validator_;
}

void OTPImportDialog::keyTextChanged()
{
   const auto strIncomplete = tr("OTP line %1 is invalid or incomplete");
   ui_->labelOtpHint->clear();
   const auto key1 = ui_->lineEditOtp1->text();
   const auto key2 = ui_->lineEditOtp2->text();
   const bool key1valid = (validator_->validateKey(key1) == EasyEncValidator::Valid);
   const bool key2valid = (validator_->validateKey(key2) == EasyEncValidator::Valid);
   if (!key1valid && !key1.isEmpty()) {
      ui_->labelOtpHint->setText(strIncomplete.arg(1));
   }
   else if (!key2valid && !key2.isEmpty()) {
      ui_->labelOtpHint->setText(strIncomplete.arg(2));
   }

   if (key1valid && !key1.isEmpty() && !key2valid) {
      ui_->lineEditOtp2->setFocus();
   }

   keyIsValid_ = (key1valid && key2valid);
   if (!keyIsValid_) {
      return;
   }

   try {
      hexKey_ = easyCodec_->toHex(EasyCoDec::Data{ ui_->lineEditOtp1->text().toStdString(), ui_->lineEditOtp2->text().toStdString() });
      ui_->lineEditPwd1->setFocus();
      onPasswordChanged();
   }
   catch (const std::exception &e) {
      ui_->labelOtpHint->setText(tr("Failed to get OTP key: %1").arg(QLatin1String(e.what())));
      keyIsValid_ = false;
      ui_->lineEditOtp1->setFocus();
   }
}

void OTPImportDialog::onPasswordChanged()
{
   const auto pwd1 = ui_->lineEditPwd1->text().toStdString();
   const auto pwd2 = ui_->lineEditPwd2->text().toStdString();
   if (pwd1.empty() || pwd2.empty()) {
      if (pwd1.empty()) {
         ui_->labelPwdHint->setText(tr("Enter OTP password"));
      }
      else if (pwd2.empty()) {
         ui_->labelPwdHint->setText(tr("Repeat OTP password"));
      }
   }
   else if (pwd1 != pwd2) {
      ui_->labelPwdHint->setText(tr("Passwords don't match"));
   }
   else {
      ui_->labelPwdHint->clear();
      ui_->pushButtonOk->setEnabled(keyIsValid_);
      return;
   }
   ui_->pushButtonOk->setEnabled(false);
}

void OTPImportDialog::accept()
{
   if (!keyIsValid_) {
      reject();
      return;
   }

   const auto otpKey = SecureBinaryData(BinaryData::CreateFromHex(hexKey_));
   auto resultCode = otpManager_->ImportOTPForCurrentUser(otpKey, ui_->lineEditPwd1->text().toStdString());
   if (resultCode != OTPManager::OTPImportResult::Success) {

      QString errorText;
      switch(resultCode) {
      case OTPManager::OTPImportResult::InvalidKey:
         errorText = tr("Invalid OTP key entered");
         break;
      case OTPManager::OTPImportResult::FileError:
         errorText = tr("Failed to save OTP file");
         break;
      case OTPManager::OTPImportResult::OutdatedOTP:
         errorText = tr("You are trying to import outdated OTP");
         break;
      }

      MessageBoxCritical(tr("OTP Import"), errorText, this).exec();
      return;
   }

   QDialog::accept();
}
