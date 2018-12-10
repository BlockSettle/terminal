#include "WalletKeysCreateWidget.h"
#include "ui_WalletKeysCreateWidget.h"
#include <set>
#include <QSpinBox>
#include "ApplicationSettings.h"
#include "MobileUtils.h"
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

WalletKeysCreateWidget::~WalletKeysCreateWidget() = default;

void WalletKeysCreateWidget::setFlags(Flags flags)
{
   flags_ = flags;
}

void WalletKeysCreateWidget::init(MobileClient::RequestType requestType
   , const std::string &walletId, const QString& username
   , const std::shared_ptr<ApplicationSettings>& appSettings)
{
   requestType_ = requestType;

   widgets_.clear();
   pwdData_.clear();

   if (flags_ & HideGroupboxCaption) {
      ui_->groupBox->setTitle(QString());
   }

   walletId_ = walletId;
   username_ = username;
   appSettings_ = appSettings;
   
   addPasswordKey();

   if (flags_ & HideWidgetContol) {
      ui_->widgetControl->hide();
   }

   for (auto& widget : widgets_) {
      widget->init(appSettings, username);
   }
}

void WalletKeysCreateWidget::addKey(bool password)
{
   assert(!walletId_.empty());
   const auto &authKeys = appSettings_->GetAuthKeys();
   auto widget = new WalletKeyWidget(requestType_, walletId_, widgets_.size(), password
      , authKeys, this);
   widget->init(appSettings_, username_);
   if (flags_ & HideAuthConnectButton) {
      widget->setHideAuthConnect(true);
   }
   if (flags_ & SetPasswordLabelAsNew) {
      widget->setPasswordLabelAsNew();
   }

   if (flags_ & HidePubKeyFingerprint || true) {
      ui_->labelPubKeyFP->hide();
   }
   else {
      const auto &pubKeyFP = autheid::toHexWithSeparators(autheid::getPublicKeyFingerprint(authKeys.second));
      ui_->labelPubKeyFP->setText(QString::fromStdString(pubKeyFP));
   }

   connect(widget, &WalletKeyWidget::keyTypeChanged, this, &WalletKeysCreateWidget::onKeyTypeChanged);
   connect(widget, &WalletKeyWidget::keyChanged, this, &WalletKeysCreateWidget::onKeyChanged);
   connect(widget, &WalletKeyWidget::encKeyChanged, this, &WalletKeysCreateWidget::onEncKeyChanged);
   connect(widget, &WalletKeyWidget::failed, this, &WalletKeysCreateWidget::failed);
   ui_->groupBox->layout()->addWidget(widget);
   ui_->pushButtonDelKey->setEnabled(true);
   widgets_.emplace_back(widget);
   pwdData_.push_back({ {}, password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Auth, {} });
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
   if (widgets_.empty()) {
      return;
   }
   widgets_.pop_back();
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
   pwdData_[index].encType = password ? bs::wallet::EncryptionType::Password : bs::wallet::EncryptionType::Auth;
   pwdData_[index].password.clear();
   emit keyChanged();
   emit keyTypeChanged(password);
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
      if (pwd.encType == bs::wallet::EncryptionType::Auth) {
         if (pwd.encKey.isNull()) {
            return false;
         }
         if (encKeys.find(pwd.encKey) != encKeys.end()) {
            return false;
         }
         encKeys.insert(pwd.encKey);
      } else if (pwd.password.getSize() < 6) {
         // Password must be at least 6 chars long.
         return false;
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
