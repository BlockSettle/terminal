#include "WalletKeysSubmitWidget.h"
#include "ui_WalletKeysSubmitWidget.h"
#include <set>
#include <QFrame>
#include <QtConcurrent/QtConcurrentRun>
#include "ApplicationSettings.h"
#include "MobileUtils.h"
#include "WalletKeyWidget.h"


WalletKeysSubmitWidget::WalletKeysSubmitWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeysSubmitWidget())
   , suspended_(false)
{
   ui_->setupUi(this);
}

WalletKeysSubmitWidget::~WalletKeysSubmitWidget() = default;

void WalletKeysSubmitWidget::setFlags(Flags flags)
{
   flags_ = flags;
}

void WalletKeysSubmitWidget::init(AutheIDClient::RequestType requestType
   , const std::string &walletId
   , bs::wallet::KeyRank keyRank
   , const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const QString &prompt)
{
   requestType_ = requestType;
   appSettings_ = appSettings;

   qDeleteAll(widgets_.cbegin(), widgets_.cend());
   widgets_.clear();
   pwdData_.clear();

   if (flags_ & HideGroupboxCaption) {
      ui_->groupBox->setTitle(QString());
   }

   walletId_ = walletId;
   if (encTypes.empty()) {
      return;
   }

   bool hasAuth = false;
   for (const auto &encType : encTypes) {
      if (encType == bs::wallet::EncryptionType::Auth) {
         hasAuth = true;
         break;
      }
   }
   if ((flags_ & HidePubKeyFingerprint) || !hasAuth || true) {
      ui_->labelPubKeyFP->hide();
   }
   else {
      ui_->labelPubKeyFP->show();
      QtConcurrent::run([this] {
         const auto &authKeys = appSettings_->GetAuthKeys();
         const auto &pubKeyFP = autheid::getPublicKeyFingerprint(authKeys.second);
         const auto &sPubKeyFP = QString::fromStdString(autheid::toHexWithSeparators(pubKeyFP));
         QMetaObject::invokeMethod(this, [this, sPubKeyFP] {
            ui_->labelPubKeyFP->setText(sPubKeyFP);
         });
      });
   }

   bool isAuthOnly = true;
   for (auto encType : encTypes) {
      if (encType != bs::wallet::EncryptionType::Auth) {
         isAuthOnly = false;
      }
   }

   int encKeyIndex = 0;
   if (isAuthOnly) {
      addKey(false, encKeys, 0, true, prompt);
   } else if (encTypes.size() == keyRank.first) {
      for (const auto &encType : encTypes) {
         const bool isPassword = (encType == bs::wallet::EncryptionType::Password);
         addKey(isPassword, encKeys, isPassword ? 0 : encKeyIndex++, true, prompt);
      }
   } else {
      if ((encTypes.size() > 1) && (keyRank.first == 1)) {
         addKey(true, encKeys, 0, false, prompt);
      }
      else {
         if ((encTypes.size() == 1) && (encTypes[0] == bs::wallet::EncryptionType::Auth) && (encKeys.size() == keyRank.first)) {
            for (unsigned int i = 0; i < keyRank.first; ++i) {
               addKey(false, encKeys, encKeyIndex++, true, prompt);
            }
         }
         else if ((encTypes.size() == 1) && (encTypes[0] == bs::wallet::EncryptionType::Password)) {
            for (unsigned int i = 0; i < keyRank.first; ++i) {
               addKey(true, encKeys, 0, true, prompt);
            }
         }
         else {
            for (unsigned int i = 0; i < keyRank.first; ++i) {
               const bool isPassword = !(encKeyIndex < encKeys.size());
               addKey(isPassword, encKeys, isPassword ? 0 : encKeyIndex++, false, prompt);
            }
         }
      }
   }
}

void WalletKeysSubmitWidget::addKey(bool password, const std::vector<SecureBinaryData> &encKeys
   , int encKeyIndex, bool isFixed, const QString &prompt)
{
   assert(!walletId_.empty());
   if (!widgets_.empty()) {
      const auto separator = new QFrame(this);
      separator->setFrameShape(QFrame::HLine);
      ui_->groupBox->layout()->addWidget(separator);
   }

   const auto &authKeys = appSettings_->GetAuthKeys();
   auto widget = new WalletKeyWidget(requestType_, walletId_, pwdData_.size(), password, authKeys, this);
   widget->init(appSettings_, QString());
   connect(widget, &WalletKeyWidget::keyTypeChanged, this, &WalletKeysSubmitWidget::onKeyTypeChanged);
   connect(widget, &WalletKeyWidget::keyChanged, this, &WalletKeysSubmitWidget::onKeyChanged);
   connect(widget, &WalletKeyWidget::encKeyChanged, this, &WalletKeysSubmitWidget::onEncKeyChanged);
   connect(widget, &WalletKeyWidget::failed, this, &WalletKeysSubmitWidget::failed);

   if (flags_ & HideAuthConnectButton) {
      widget->setHideAuthConnect(true);
   }
   if (flags_ & HideAuthCombobox) {
      widget->setHideAuthCombobox(true);
   }
   if (flags_ & AuthProgressBarFixed) {
      widget->setProgressBarFixed(true);
   }
   if (flags_ & AuthIdVisible) {
      widget->setShowAuthId(true);
   }
   if (flags_ & SetPasswordLabelAsOld) {
      widget->setPasswordLabelAsOld();
   }
   if (flags_ & HideAuthEmailLabel) {
      widget->setHideAuthEmailLabel(true);
   }
   if (flags_ & HideAuthControlsOnSignClicked) {
      widget->setHideAuthControlsOnSignClicked(true);
   }
   if (flags_ & HideProgressBar) {
      widget->setHideProgressBar(true);
   }
   if (flags_ & HidePasswordWarning) {
      widget->setHidePasswordWarning(true);
   }
   ui_->groupBox->layout()->addWidget(widget);

   widgets_.push_back(widget);
   pwdData_.push_back({ {}, password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Auth, {} });
   emit keyCountChanged();
   widget->setEncryptionKeys(encKeys, encKeyIndex);
   widget->setFixedType(isFixed);
   if (isFixed && !suspended_) {
      widget->start();
   }
}

void WalletKeysSubmitWidget::setFocus()
{
   if (widgets_.empty()) {
      return;
   }
   widgets_.front()->setFocus();
}

void WalletKeysSubmitWidget::onKeyChanged(int index, SecureBinaryData key)
{
   if ((index < 0) || (index >= pwdData_.size())) {
      return;
   }
   pwdData_[index].password = key;
   isKeyFinal_ = (pwdData_[index].encType == bs::wallet::EncryptionType::Auth);
   emit keyChanged();
}

void WalletKeysSubmitWidget::onKeyTypeChanged(int index, bool password)
{
   if ((index < 0) || (index >= pwdData_.size())) {
      return;
   }
   pwdData_[index].encType = password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Auth;
   pwdData_[index].password.clear();
   emit keyChanged();
}

void WalletKeysSubmitWidget::onEncKeyChanged(int index, SecureBinaryData encKey)
{
   if ((index < 0) || (index >= pwdData_.size())) {
      return;
   }
   pwdData_[index].encKey = encKey;
   emit keyChanged();
}

bool WalletKeysSubmitWidget::isValid() const
{
   if (pwdData_.empty()) {
      return true;
   }
   std::set<SecureBinaryData> encKeys;
   for (const auto &pwd : pwdData_) {
      if (pwd.password.isNull()) {
         return false;
      }
      if (pwd.encType == bs::wallet::EncryptionType::Auth) {
         if (pwd.encKey.isNull()) {
            return false;
         }
         if (encKeys.find(pwd.encKey) != encKeys.end()) {
            return false;
         }
         encKeys.insert(pwd.encKey);
      }
   }
   return true;
}

void WalletKeysSubmitWidget::cancel()
{
   for (const auto &keyWidget : widgets_) {
      keyWidget->cancel();
   }
}

std::string WalletKeysSubmitWidget::encKey(int index) const
{
   if (index < 0 || index >= pwdData_.size()) {
      return {};
   }
   return pwdData_[index].encKey.toBinStr();
}

SecureBinaryData WalletKeysSubmitWidget::key() const
{
   SecureBinaryData result;
   for (const auto &pwd : pwdData_) {
      result = mergeKeys(result, pwd.password);
   }
   return result;
}

bool WalletKeysSubmitWidget::isKeyFinal() const
{
   return isKeyFinal_;
}

void WalletKeysSubmitWidget::resume()
{
   suspended_ = false;
   for (const auto &widget : widgets_) {
      widget->start();
   }
}
