#include "WalletKeysSubmitFrejaDialog.h"

#include <QGraphicsColorizeEffect>
#include <QPropertyAnimation>
#include <QString>
#include <spdlog/spdlog.h>
#include "ui_WalletKeysSubmitFrejaDialog.h"

namespace {

   const int kAnimationDurationMs = 500;

   const QColor kSuccessColor = Qt::green;
   const QColor kFailColor = Qt::red;

}

WalletKeysSubmitFrejaDialog::WalletKeysSubmitFrejaDialog(const std::string &walletId
   , bs::wallet::KeyRank keyRank, const std::vector<bs::wallet::EncryptionType> &encTypes
   , const std::vector<SecureBinaryData> &encKeys, const QString &prompt, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::WalletKeysSubmitFrejaDialog)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &WalletKeysSubmitFrejaDialog::cancel);

   QString address = !encKeys.empty() ? QString::fromStdString(encKeys[0].toBinStr()) : QString();

   ui_->labelAction->setText(prompt);
   ui_->labelWalletId->setText(tr("Wallet ID: %1").arg(QString::fromStdString(walletId)));
   ui_->labelEmail->setText(address);

   ui_->widgetKeysSubmit->setFlags(WalletKeysSubmitWidget::HideFrejaCombobox 
      | WalletKeysSubmitWidget::HideFrejaConnectButton
      | WalletKeysSubmitWidget::HideGroupboxCaption);
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
   QPropertyAnimation *a = startAnimation(true);
   connect(a, &QPropertyAnimation::finished, this, &WalletKeysSubmitFrejaDialog::accept);
}

void WalletKeysSubmitFrejaDialog::onFailed()
{
   QPropertyAnimation *a = startAnimation(false);
   connect(a, &QPropertyAnimation::finished, this, &WalletKeysSubmitFrejaDialog::reject);
}

void WalletKeysSubmitFrejaDialog::cancel()
{
   reject();
}

QPropertyAnimation* WalletKeysSubmitFrejaDialog::startAnimation(bool success)
{
   QGraphicsColorizeEffect *eff = new QGraphicsColorizeEffect(this);
   eff->setColor(success ? kSuccessColor : kFailColor);
   ui_->labelEmail->setGraphicsEffect(eff);

   QPropertyAnimation *a = new QPropertyAnimation(eff, "strength");
   a->setDuration(kAnimationDurationMs);
   a->setStartValue(0.0);
   a->setEndValue(1.0);
   a->setEasingCurve(QEasingCurve::Linear);
   a->start(QPropertyAnimation::DeleteWhenStopped);

   return a;
}
