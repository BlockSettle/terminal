#include "WalletKeysSubmitWidget.h"
#include "ui_WalletKeysSubmitWidget.h"
#include <set>
#include <QFrame>
#include "WalletKeyWidget.h"


WalletKeysSubmitWidget::WalletKeysSubmitWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeysSubmitWidget())
   , suspended_(false)
{
   ui_->setupUi(this);
}

void WalletKeysSubmitWidget::init(const std::string &walletId, bs::wallet::KeyRank keyRank
   , const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys)
{
   walletId_ = walletId;
   if (encTypes.empty()) {
      return;
   }
   int encKeyIndex = 0;
   if (encTypes.size() == keyRank.first) {
      for (const auto &encType : encTypes) {
         const bool isPassword = (encType == bs::wallet::EncryptionType::Password);
         addKey(isPassword, encKeys, isPassword ? 0 : encKeyIndex++, true);
      }
   }
   else {
      if ((encTypes.size() > 1) && (keyRank.first == 1)) {
         addKey(true, encKeys);
      }
      else {
         if ((encTypes.size() == 1) && (encTypes[0] == bs::wallet::EncryptionType::Freja) && (encKeys.size() == keyRank.first)) {
            for (unsigned int i = 0; i < keyRank.first; ++i) {
               addKey(false, encKeys, encKeyIndex++, true);
            }
         }
         else if ((encTypes.size() == 1) && (encTypes[0] == bs::wallet::EncryptionType::Password)) {
            for (unsigned int i = 0; i < keyRank.first; ++i) {
               addKey(true, encKeys, 0, true);
            }
         }
         else {
            for (unsigned int i = 0; i < keyRank.first; ++i) {
               const bool isPassword = !(encKeyIndex < encKeys.size());
               addKey(isPassword, encKeys, isPassword ? 0 : encKeyIndex++);
            }
         }
      }
   }
}

void WalletKeysSubmitWidget::addKey(bool password, const std::vector<SecureBinaryData> &encKeys
   , int encKeyIndex, bool isFixed)
{
   assert(!walletId_.empty());
   if (!widgets_.empty()) {
      const auto separator = new QFrame(this);
      separator->setFrameShape(QFrame::HLine);
      ui_->groupBox->layout()->addWidget(separator);
   }

   auto widget = new WalletKeyWidget(walletId_, pwdData_.size(), password, this);
   connect(widget, &WalletKeyWidget::keyTypeChanged, this, &WalletKeysSubmitWidget::onKeyTypeChanged);
   connect(widget, &WalletKeyWidget::keyChanged, this, &WalletKeysSubmitWidget::onKeyChanged);
   connect(widget, &WalletKeyWidget::encKeyChanged, this, &WalletKeysSubmitWidget::onEncKeyChanged);
   ui_->groupBox->layout()->addWidget(widget);

   widgets_.push_back(widget);
   pwdData_.push_back({ {}, password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Freja, {} });
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
   emit keyChanged();
}

void WalletKeysSubmitWidget::onKeyTypeChanged(int index, bool password)
{
   if ((index < 0) || (index >= pwdData_.size())) {
      return;
   }
   pwdData_[index].encType = password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Freja;
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
      if (pwd.encType == bs::wallet::EncryptionType::Freja) {
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

SecureBinaryData WalletKeysSubmitWidget::key() const
{
   SecureBinaryData result;
   for (const auto &pwd : pwdData_) {
      result = xor(result, pwd.password);
   }
   return result;
}

void WalletKeysSubmitWidget::resume()
{
   suspended_ = false;
   for (const auto &widget : widgets_) {
      widget->start();
   }
}
