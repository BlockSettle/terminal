#include "EnterWalletPassword.h"
#include "ui_EnterWalletPassword.h"
#include <spdlog/spdlog.h>


EnterWalletPassword::EnterWalletPassword(const std::string &walletId
   , bs::wallet::KeyRank keyRank, const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys, const QString &prompt
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::EnterWalletPassword())
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterWalletPassword::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterWalletPassword::reject);

   connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });

   ui_->labelAction->setText(prompt);
   ui_->labelWalletId->setText(tr("Wallet ID: %1").arg(QString::fromStdString(walletId)));

   if (encTypes.size() == 1 && encTypes[0] == bs::wallet::EncryptionType::Freja) {
      ui_->widgetSubmitKeys->setFlags(WalletKeysSubmitWidget::HideFrejaConnectButton
         | WalletKeysSubmitWidget::HideFrejaCombobox
         | WalletKeysSubmitWidget::HideGroupboxCaption
         | WalletKeysSubmitWidget::FrejaProgressBarFixed
         | WalletKeysSubmitWidget::FrejaIdVisible);

      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, this, &EnterWalletPassword::accept);
      connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::failed, this, &EnterWalletPassword::reject);

      ui_->pushButtonOk->hide();
   }

   ui_->widgetSubmitKeys->init(walletId, keyRank, encTypes, encKeys, prompt);
   ui_->widgetSubmitKeys->setFocus();

   updateState();
   
   adjustSize();
   setMinimumSize(size());
}

EnterWalletPassword::~EnterWalletPassword() = default;

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
