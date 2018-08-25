#include "WalletKeysSubmitFrejaDialog.h"

#include <QGraphicsColorizeEffect>
#include <QPropertyAnimation>
#include <QString>
#include <spdlog/spdlog.h>
#include "ui_WalletKeysSubmitFrejaDialog.h"

namespace {

const int kTimeoutSec = 120;
const int kUpdateIntervalMs = 500;

const int kAnimationDurationMs = 500;

const QColor kSuccessColor = Qt::green;
const QColor kFailColor = Qt::red;

}

WalletKeysSubmitFrejaDialog::WalletKeysSubmitFrejaDialog(const QString& walletName, const std::string &walletId
    , const std::vector<SecureBinaryData> &encKeys, const QString &prompt, QWidget* parent) :
  QDialog(parent),
  ui_(new Ui::WalletKeysSubmitFrejaDialog)
  , frejaSign_(spdlog::get(""), 1)
{
  ui_->setupUi(this);

  connect(&frejaSign_, &FrejaSignWallet::succeeded, this, &WalletKeysSubmitFrejaDialog::onFrejaSucceeded);
  connect(&frejaSign_, &FrejaSign::failed, this, &WalletKeysSubmitFrejaDialog::onFrejaFailed);

  connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &WalletKeysSubmitFrejaDialog::cancel);

  timer_.setInterval(kUpdateIntervalMs);
  connect(&timer_, &QTimer::timeout, this, &WalletKeysSubmitFrejaDialog::onTimer);

  QString address = !encKeys.empty() ? QString::fromStdString(encKeys[0].toBinStr()) : QString();

  ui_->labelAction->setText(prompt);
  ui_->labelWalletId->setText(tr("Wallet ID: %1").arg(QString::fromStdString(walletId)));
  ui_->labelEmail->setText(address);

  frejaSign_.start(address, tr("Activate Freja eID signing"), walletId);

  timer_.start();
  timeout_.start();

  ui_->progressBarTimeout->setMaximum(kTimeoutSec);
  ui_->progressBarTimeout->setValue(kTimeoutSec);
}

WalletKeysSubmitFrejaDialog::~WalletKeysSubmitFrejaDialog() {
  delete ui_;
}

SecureBinaryData WalletKeysSubmitFrejaDialog::GetPassword() const {
  return password_;
}

void WalletKeysSubmitFrejaDialog::onFrejaSucceeded(SecureBinaryData password)
{
  password_ = password;

  QPropertyAnimation *a = startAnimation(true);
  connect(a, &QPropertyAnimation::finished, this, &WalletKeysSubmitFrejaDialog::accept);
}

void WalletKeysSubmitFrejaDialog::onFrejaFailed(const QString &text)
{
  QPropertyAnimation *a = startAnimation(false);
  connect(a, &QPropertyAnimation::finished, this, &WalletKeysSubmitFrejaDialog::reject);
}

void WalletKeysSubmitFrejaDialog::onTimer()
{
  int timeLeftSec = (kTimeoutSec - timeout_.elapsed() / 1000);
  if (timeLeftSec <= 0) {
    cancel();
  } else {
    ui_->progressBarTimeout->setFormat(tr("%1 seconds left").arg(timeLeftSec));
    ui_->progressBarTimeout->setValue(timeLeftSec);
  }
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
