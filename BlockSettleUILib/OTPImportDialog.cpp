#include "OTPImportDialog.h"
#include "ui_OTPImportDialog.h"

#include "EasyCoDec.h"
#include "EasyEncValidator.h"
#include "EncryptionUtils.h"
#include "MessageBoxCritical.h"
#include "OTPFile.h"
#include "OTPManager.h"

#include <spdlog/spdlog.h>


OTPImportDialog::OTPImportDialog(const std::shared_ptr<OTPManager>& otpManager,
   const std::string &defaultUserName, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::OTPImportDialog())
   , otpManager_(otpManager)
   , easyCodec_(std::make_shared<EasyCoDec>())
   , frejaSign_(spdlog::get(""))
{
   ui_->setupUi(this);

   ui_->pushButtonOk->setEnabled(false);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &OTPImportDialog::accept);

   validator_ = new EasyEncValidator(easyCodec_);
   ui_->lineEditOtp1->setValidator(validator_);
   connect(ui_->lineEditOtp1, &QLineEdit::textEdited, this, &OTPImportDialog::keyTextChanged);
   connect(ui_->lineEditOtp1, &QLineEdit::editingFinished, this, &OTPImportDialog::keyTextChanged);
   ui_->lineEditOtp2->setValidator(validator_);
   connect(ui_->lineEditOtp2, &QLineEdit::textEdited, this, &OTPImportDialog::keyTextChanged);
   connect(ui_->lineEditOtp2, &QLineEdit::editingFinished, this, &OTPImportDialog::keyTextChanged);

   connect(ui_->lineEditFrejaId, &QLineEdit::textChanged, this, &OTPImportDialog::onFrejaIdChanged);
   connect(ui_->pushButtonFreja, &QPushButton::clicked, this, &OTPImportDialog::startFrejaSign);

   connect(&frejaSign_, &FrejaSignOTP::succeeded, this, &OTPImportDialog::onFrejaSucceeded);
   connect(&frejaSign_, &FrejaSign::failed, this, &OTPImportDialog::onFrejaFailed);
   connect(&frejaSign_, &FrejaSign::statusUpdated, this, &OTPImportDialog::onFrejaStatusUpdated);

   ui_->lineEditFrejaId->setText(QString::fromStdString(defaultUserName));
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
      ui_->lineEditFrejaId->setFocus();
      updateAcceptButton();
   }
   catch (const std::exception &e) {
      ui_->labelOtpHint->setText(tr("Failed to get OTP key: %1").arg(QLatin1String(e.what())));
      keyIsValid_ = false;
      ui_->lineEditOtp1->setFocus();
   }
}

void OTPImportDialog::onFrejaIdChanged(const QString &)
{
   updateAcceptButton();
}

void OTPImportDialog::startFrejaSign()
{
   frejaSign_.start(ui_->lineEditFrejaId->text(), tr("Activate Freja eID signing"),
      OTPFile::CreateFromPrivateKey(spdlog::get(""), QString(),
         SecureBinaryData(BinaryData::CreateFromHex(hexKey_)),
         bs::wallet::EncryptionType::Unencrypted)->GetShortId());
   ui_->pushButtonFreja->setEnabled(false);
   ui_->lineEditFrejaId->setEnabled(false);
}

void OTPImportDialog::onFrejaSucceeded(SecureBinaryData password)
{
   ui_->labelPwdHint->setText(tr("Successfully signed"));
   otpPassword_ = password;
   updateAcceptButton();
}

void OTPImportDialog::onFrejaFailed(const QString &text)
{
   ui_->pushButtonFreja->setEnabled(true);
   ui_->labelPwdHint->setText(tr("Freja failed: %1").arg(text));
}

void OTPImportDialog::onFrejaStatusUpdated(const QString &status)
{
   ui_->labelPwdHint->setText(status);
}

void OTPImportDialog::updateAcceptButton()
{
   bool enable = false;

   if (keyIsValid_ && !otpPassword_.isNull())
      enable = true;

   ui_->pushButtonFreja->setEnabled(!ui_->lineEditFrejaId->text().isEmpty() && keyIsValid_);

   ui_->pushButtonOk->setEnabled(enable);
}

void OTPImportDialog::accept()
{
   if (!keyIsValid_) {
      reject();
      return;
   }

   const auto otpKey = SecureBinaryData(BinaryData::CreateFromHex(hexKey_));
   auto resultCode = otpManager_->ImportOTPForCurrentUser(otpKey,
      otpPassword_, bs::wallet::EncryptionType::Freja, ui_->lineEditFrejaId->text().toStdString());
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
      default:
         break;
      }

      MessageBoxCritical(tr("OTP Import"), errorText, this).exec();
      return;
   }

   QDialog::accept();
}
