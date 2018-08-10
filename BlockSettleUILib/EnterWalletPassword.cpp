#include "EnterWalletPassword.h"
#include "ui_EnterWalletPassword.h"
#include <spdlog/spdlog.h>


EnterWalletPassword::EnterWalletPassword(const QString& walletName, const std::string &walletId
   , bs::hd::KeyRank keyRank, const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys, const QString &prompt
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::EnterWalletPassword())
{
   ui_->setupUi(this);

   ui_->labelWalletName->setText(walletName);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &EnterWalletPassword::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &EnterWalletPassword::reject);

   connect(ui_->widgetSubmitKeys, &WalletKeysSubmitWidget::keyChanged, [this] { updateState(); });
   ui_->widgetSubmitKeys->init(walletId, keyRank, encTypes, encKeys);
   ui_->widgetSubmitKeys->setFocus();

   updateState();
}

void EnterWalletPassword::updateState()
{
   ui_->pushButtonOk->setEnabled(ui_->widgetSubmitKeys->isValid());
}

void EnterWalletPassword::showEvent(QShowEvent *evt)
{
   QApplication::processEvents();
   adjustSize();
   QDialog::showEvent(evt);
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
