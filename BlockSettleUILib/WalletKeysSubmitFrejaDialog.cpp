#include "WalletKeysSubmitFrejaDialog.h"

#include <QGraphicsColorizeEffect>
#include <QString>
#include <spdlog/spdlog.h>
#include "ui_WalletKeysSubmitFrejaDialog.h"


WalletKeysSubmitFrejaDialog::WalletKeysSubmitFrejaDialog(const std::string &walletId
   , bs::wallet::KeyRank keyRank, const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys, const QString &prompt, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::WalletKeysSubmitFrejaDialog)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &WalletKeysSubmitFrejaDialog::cancel);

   ui_->labelAction->setText(prompt);
   ui_->labelWalletId->setText(tr("Wallet ID: %1").arg(QString::fromStdString(walletId)));

   ui_->widgetKeysSubmit->setFlags(WalletKeysSubmitWidget::HideFrejaCombobox 
      | WalletKeysSubmitWidget::HideFrejaConnectButton
      | WalletKeysSubmitWidget::HideGroupboxCaption
      | WalletKeysSubmitWidget::FrejaProgressBarFixed
      | WalletKeysSubmitWidget::FrejaIdVisible
      | WalletKeysSubmitWidget::HideFrejaCombobox);
   ui_->widgetKeysSubmit->init(walletId, keyRank, encTypes, encKeys);

   connect(ui_->widgetKeysSubmit, &WalletKeysSubmitWidget::keyChanged, this, &WalletKeysSubmitFrejaDialog::onSucceeded);
   connect(ui_->widgetKeysSubmit, &WalletKeysSubmitWidget::failed, this, &WalletKeysSubmitFrejaDialog::onFailed);
}

WalletKeysSubmitFrejaDialog::~WalletKeysSubmitFrejaDialog() = default;

SecureBinaryData WalletKeysSubmitFrejaDialog::GetPassword() const {
   return ui_->widgetKeysSubmit->key();
}

void WalletKeysSubmitFrejaDialog::onSucceeded()
{
   accept();
}

void WalletKeysSubmitFrejaDialog::onFailed()
{
   reject();
}

void WalletKeysSubmitFrejaDialog::cancel()
{
   reject();
}
