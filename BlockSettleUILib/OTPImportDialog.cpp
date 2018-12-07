#include "OTPImportDialog.h"
#include "ui_OTPImportDialog.h"

#include "ApplicationSettings.h"
#include "EasyCoDec.h"
#include "EasyEncValidator.h"
#include "EncryptionUtils.h"
#include "BSMessageBox.h"
#include "OTPFile.h"
#include "OTPManager.h"
#include "MobileClient.h"
#include "UiUtils.h"
#include "make_unique.h"

#include <spdlog/spdlog.h>


OTPImportDialog::OTPImportDialog(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<OTPManager>& otpManager
   , const std::string &defaultUserName
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::OTPImportDialog())
   , logger_(logger)
   , otpManager_(otpManager)
   , easyCodec_(std::make_shared<EasyCoDec>())
{
   ui_->setupUi(this);

   ui_->pushButtonOk->setEnabled(false);
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &OTPImportDialog::accept);

   validator_ = make_unique<EasyEncValidator>(easyCodec_);
   ui_->lineEditOtp1->setValidator(validator_.get());
   connect(ui_->lineEditOtp1, &QLineEdit::textEdited, this, &OTPImportDialog::keyTextChanged);
   connect(ui_->lineEditOtp1, &QLineEdit::editingFinished, this, &OTPImportDialog::keyTextChanged);
   ui_->lineEditOtp2->setValidator(validator_.get());
   connect(ui_->lineEditOtp2, &QLineEdit::textEdited, this, &OTPImportDialog::keyTextChanged);
   connect(ui_->lineEditOtp2, &QLineEdit::editingFinished, this, &OTPImportDialog::keyTextChanged);

   connect(ui_->lineEditAuthId, &QLineEdit::textChanged, this, &OTPImportDialog::onAuthIdChanged);
   connect(ui_->pushButtonAuth, &QPushButton::clicked, this, &OTPImportDialog::startAuthSign);

   ui_->lineEditAuthId->setText(QString::fromStdString(defaultUserName));

   mobileClient_ = new MobileClient(logger_, appSettings->GetAuthKeys(), this);
   connect(mobileClient_, &MobileClient::succeeded, this, &OTPImportDialog::onAuthSucceeded);
   connect(mobileClient_, &MobileClient::failed, this, &OTPImportDialog::onAuthFailed);

   std::string serverPubKey = appSettings->get<std::string>(ApplicationSettings::authServerPubKey);
   std::string serverHost = appSettings->get<std::string>(ApplicationSettings::authServerHost);
   std::string serverPort = appSettings->get<std::string>(ApplicationSettings::authServerPort);
   mobileClient_->init(serverPubKey, serverHost, serverPort);
}

OTPImportDialog::~OTPImportDialog() = default;

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

   if (!key1valid) {
      UiUtils::setWrongState(ui_->lineEditOtp1, true);
   } else {
      UiUtils::setWrongState(ui_->lineEditOtp1, false);
   }

   if (!key2valid) {
      UiUtils::setWrongState(ui_->lineEditOtp2, true);
   } else {
      UiUtils::setWrongState(ui_->lineEditOtp2, false);
   }

   keyIsValid_ = (key1valid && key2valid);
   if (!keyIsValid_) {
      return;
   }

   try {
      string otpKeyHex = easyCodec_->toHex(EasyCoDec::Data{
         ui_->lineEditOtp1->text().toStdString(), ui_->lineEditOtp2->text().toStdString() });
      otpKey_ = SecureBinaryData(BinaryData::CreateFromHex(otpKeyHex));
      ui_->lineEditAuthId->setFocus();
      updateAcceptButton();
   }
   catch (const std::exception &e) {
      ui_->labelOtpHint->setText(tr("Failed to get OTP key: %1").arg(QLatin1String(e.what())));
      keyIsValid_ = false;
      ui_->lineEditOtp1->setFocus();
   }
}

void OTPImportDialog::onAuthIdChanged(const QString &)
{
   updateAcceptButton();
}

void OTPImportDialog::startAuthSign()
{
   ui_->pushButtonAuth->setEnabled(false);
   ui_->lineEditAuthId->setEnabled(false);

   QString otpId = OTPFile::GetShortId(OTPFile::GetOtpIdFromPrivateKey(otpKey_));
   if (otpId.isEmpty()) {
      BSMessageBox(BSMessageBox::critical, tr("Error"), tr("Invalid OTP key"), this).exec();
      return;
   }

   mobileClient_->start(MobileClient::ActivateOTP
      , ui_->lineEditAuthId->text().toStdString(), otpId.toStdString(), {});
}

void OTPImportDialog::onAuthSucceeded(const std::string& encKey, const SecureBinaryData &password)
{
   ui_->labelPwdHint->setText(tr("Successfully signed"));
   otpPassword_ = password;
   updateAcceptButton();
}

void OTPImportDialog::onAuthFailed(const QString &text)
{
   ui_->pushButtonAuth->setEnabled(true);
   ui_->labelPwdHint->setText(tr("Auth failed: %1").arg(text));
}

void OTPImportDialog::onAuthStatusUpdated(const QString &status)
{
   ui_->labelPwdHint->setText(status);
}

void OTPImportDialog::updateAcceptButton()
{
   bool enable = false;

   if (keyIsValid_ && !otpPassword_.isNull())
      enable = true;

   ui_->pushButtonAuth->setEnabled(!ui_->lineEditAuthId->text().isEmpty() && keyIsValid_);

   ui_->pushButtonOk->setEnabled(enable);
}

void OTPImportDialog::accept()
{
   if (!keyIsValid_) {
      reject();
      return;
   }

   auto resultCode = otpManager_->ImportOTPForCurrentUser(otpKey_,
      otpPassword_, bs::wallet::EncryptionType::Auth, ui_->lineEditAuthId->text().toStdString());
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

      BSMessageBox(BSMessageBox::critical, tr("OTP Import"), errorText, this).exec();
      return;
   }

   QDialog::accept();
}
