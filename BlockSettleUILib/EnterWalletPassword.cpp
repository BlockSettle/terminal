#include "EnterWalletPassword.h"
#include "ui_EnterWalletPassword.h"
#include <spdlog/spdlog.h>



EnterWalletPassword::EnterWalletPassword(MobileClientRequest requestType, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::EnterWalletPassword())
   , requestType_(requestType)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterWalletPassword::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterWalletPassword::reject);

   connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
}

EnterWalletPassword::~EnterWalletPassword() = default;

void EnterWalletPassword::init(const std::string &walletId, bs::wallet::KeyRank keyRank
   , const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const QString &prompt, const QString &title)
{
   ui_->labelAction->setText(prompt);
   ui_->labelWalletId->setText(tr("Wallet ID: %1").arg(QString::fromStdString(walletId)));

   if (!title.isEmpty()) {
      setWindowTitle(title);
   }

   if (encTypes.size() == 1 && encTypes[0] == bs::wallet::EncryptionType::Auth) {
      ui_->widgetSubmitKeys->setFlags(WalletKeysSubmitWidget::HideAuthConnectButton
         | WalletKeysSubmitWidget::HideAuthCombobox
         | WalletKeysSubmitWidget::HideGroupboxCaption
         | WalletKeysSubmitWidget::AuthProgressBarFixed
         | WalletKeysSubmitWidget::AuthIdVisible);

      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, this, &EnterWalletPassword::accept);
      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::failed, this, &EnterWalletPassword::reject);

      ui_->pushButtonOk->hide();
   }

   ui_->widgetSubmitKeys->init(requestType_, walletId, keyRank, encTypes, encKeys, appSettings, prompt);
   ui_->widgetSubmitKeys->setFocus();

   updateState();

   adjustSize();
   setMinimumSize(size());
}

void EnterWalletPassword::init(const std::string &walletId, bs::wallet::KeyRank keyRank
   , const std::vector<bs::wallet::PasswordData> &keys
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const QString &prompt, const QString &title)
{
   std::vector<bs::wallet::EncryptionType> encTypes;
   std::vector<SecureBinaryData> encKeys;
   for (const bs::wallet::PasswordData& key : keys) {
      encTypes.push_back(key.encType);
      encKeys.push_back(key.encKey);
   }

   init(walletId, keyRank, encTypes, encKeys, appSettings, prompt, title);
}

void EnterWalletPassword::updateState()
{
   ui_->pushButtonOk->setEnabled(ui_->widgetSubmitKeys->isValid());
}

void EnterWalletPassword::reject()
{
   ui_->widgetSubmitKeys->cancel();
   QDialog::reject();
}

SecureBinaryData EnterWalletPassword::GetPassword() const
{
   return ui_->widgetSubmitKeys->key();
}

string EnterWalletPassword::getDeviceId() const
{
   return ui_->widgetSubmitKeys->getDeviceId();
}
