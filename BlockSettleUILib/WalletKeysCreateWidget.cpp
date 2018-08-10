#include "WalletKeysCreateWidget.h"
#include "ui_WalletKeysCreateWidget.h"
#include <QSpinBox>
#include "WalletKeyWidget.h"


WalletKeysCreateWidget::WalletKeysCreateWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeysCreateWidget())
{
   ui_->setupUi(this);
   ui_->pushButtonDelKey->setEnabled(false);
   ui_->labelRankN->clear();

   connect(ui_->pushButtonAddKey, &QPushButton::clicked, this, &WalletKeysCreateWidget::onAddClicked);
   connect(ui_->pushButtonDelKey, &QPushButton::clicked, this, &WalletKeysCreateWidget::onDelClicked);
   connect(ui_->spinBoxRankM, SIGNAL(valueChanged(int)), this, SLOT(updateKeyRank(int)));
}

void WalletKeysCreateWidget::init(const std::string &walletId)
{
   walletId_ = walletId;
   addPasswordKey();
}

void WalletKeysCreateWidget::addKey(bool password)
{
   assert(!walletId_.empty());
   auto widget = new WalletKeyWidget(walletId_, widgets_.size(), password, this);
   connect(widget, &WalletKeyWidget::keyTypeChanged, this, &WalletKeysCreateWidget::onKeyTypeChanged);
   connect(widget, &WalletKeyWidget::keyChanged, this, &WalletKeysCreateWidget::onKeyChanged);
   connect(widget, &WalletKeyWidget::encKeyChanged, this, &WalletKeysCreateWidget::onEncKeyChanged);
   ui_->groupBox->layout()->addWidget(widget);
   ui_->pushButtonDelKey->setEnabled(true);
   widgets_.push_back(widget);
   pwdData_.push_back({ {}, password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Freja, {} });
   ui_->spinBoxRankM->setMaximum(pwdData_.size());
   ui_->spinBoxRankM->setMinimum(1);
   updateKeyRank(0);
   emit keyCountChanged();
}

void WalletKeysCreateWidget::onAddClicked()
{
   addPasswordKey();
}

void WalletKeysCreateWidget::onDelClicked()
{
   ui_->groupBox->layout()->removeWidget(widgets_.back());
   widgets_.back()->deleteLater();
   widgets_.resize(widgets_.size() - 1);
   pwdData_.resize(widgets_.size());
   if (pwdData_.empty()) {
      ui_->spinBoxRankM->setMinimum(0);
   }
   ui_->spinBoxRankM->setMaximum(pwdData_.size());
   updateKeyRank(0);
   emit keyCountChanged();
   if (widgets_.empty()) {
      ui_->pushButtonDelKey->setEnabled(false);
   }
}

void WalletKeysCreateWidget::onKeyChanged(int index, SecureBinaryData key)
{
   if ((index < 0) || (index >= pwdData_.size())) {
      return;
   }
   pwdData_[index].password = key;
   emit keyChanged();
}

void WalletKeysCreateWidget::onKeyTypeChanged(int index, bool password)
{
   if ((index < 0) || (index >= pwdData_.size())) {
      return;
   }
   pwdData_[index].encType = password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Freja;
   pwdData_[index].password.clear();
   emit keyChanged();
}

void WalletKeysCreateWidget::onEncKeyChanged(int index, SecureBinaryData encKey)
{
   if ((index < 0) || (index >= pwdData_.size())) {
      return;
   }
   pwdData_[index].encKey = encKey;
   emit keyChanged();
}

void WalletKeysCreateWidget::updateKeyRank(int)
{
   keyRank_.second = pwdData_.size();
   keyRank_.first = ui_->spinBoxRankM->value();
   if (pwdData_.empty()) {
      ui_->labelRankN->clear();
   }
   else {
      ui_->labelRankN->setText(QString::number(keyRank_.second));
   }
}

bool WalletKeysCreateWidget::isValid() const
{
   if (pwdData_.empty()) {
      return false;
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

void WalletKeysCreateWidget::cancel()
{
   for (const auto &keyWidget : widgets_) {
      keyWidget->cancel();
   }
}
