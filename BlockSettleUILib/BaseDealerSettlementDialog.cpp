#include "BaseDealerSettlementDialog.h"
#include <QCoreApplication>
#include "HDWallet.h"
#include "SettlementContainer.h"
#include "SignContainer.h"
#include "WalletKeysSubmitWidget.h"
#include <spdlog/spdlog.h>


BaseDealerSettlementDialog::BaseDealerSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::SettlementContainer> &settlContainer, const std::shared_ptr<SignContainer> &signContainer
      , QWidget* parent)
   : QDialog(parent)
   , logger_(logger)
   , settlContainer_(settlContainer)
   , signContainer_(signContainer)
{
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStarted, this, &BaseDealerSettlementDialog::onTimerStarted);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStopped, this, &BaseDealerSettlementDialog::onTimerStopped);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerTick, this, &BaseDealerSettlementDialog::onTimerTick);
   connect(settlContainer_.get(), &bs::SettlementContainer::completed, this, &QDialog::accept);

   connect(settlContainer_.get(), &bs::SettlementContainer::error, [this](QString msg) { setCriticalHintMessage(msg); });
   connect(settlContainer_.get(), &bs::SettlementContainer::info, [this](QString msg) { setHintText(msg); });

   connect(signContainer_.get(), &SignContainer::HDWalletInfo, this, &BaseDealerSettlementDialog::onHDWalletInfo);
}

void BaseDealerSettlementDialog::connectToProgressBar(QProgressBar *progressBar)
{
   progressBar_ = progressBar;
   progressBar_->hide();
}

void BaseDealerSettlementDialog::connectToHintLabel(QLabel *hintLabel, QLabel *errorLabel)
{
   hintLabel_ = hintLabel;
   errorLabel_ = errorLabel;

   setHintText(QString{});
   errorLabel_->hide();
}

void BaseDealerSettlementDialog::setHintText(const QString& hint)
{
   QMetaObject::invokeMethod(hintLabel_, "setText", Q_ARG(QString, hint));
}

void BaseDealerSettlementDialog::setCriticalHintMessage(const QString& hint)
{
   hintSetToCritical_ = true;

   const auto formatString = tr("<span style=\"color: #CF292E;\">%1</span>");
   QString text = formatString.arg(hint);

   QMetaObject::invokeMethod(errorLabel_, "show");
   QMetaObject::invokeMethod(errorLabel_, "setText", Q_ARG(QString, text));
}

void BaseDealerSettlementDialog::setFrejaPasswordPrompt(const QString &prompt)
{
   frejaPrompt_ = prompt;
}

void BaseDealerSettlementDialog::onTimerStarted(int msDuration)
{
   progressBar_->show();
   progressBar_->setMinimum(0);
   progressBar_->setMaximum(msDuration);
   progressBar_->setValue(progressBar_->maximum());
   progressBar_->setFormat(QString());
}

void BaseDealerSettlementDialog::onTimerStopped()
{
   progressBar_->hide();
}

void BaseDealerSettlementDialog::onTimerTick(int msCurrent, int msDuration)
{
   progressBar_->setFormat(tr("%n second(s) remaining", "", (int)(msCurrent / 1000)));
   progressBar_->setValue(msCurrent);
}

void BaseDealerSettlementDialog::reject()
{
   widgetWalletKeys()->cancel();
   settlContainer_->cancel();
   QDialog::reject();
}

void BaseDealerSettlementDialog::onHDWalletInfo(unsigned int id, std::vector<bs::wallet::EncryptionType> encTypes
   , std::vector<SecureBinaryData> encKeys, bs::wallet::KeyRank keyRank)
{
   if (!infoReqId_ || (id != infoReqId_)) {
      return;
   }
   infoReqId_ = 0;
   walletInfoReceived_ = true;
   encTypes_ = encTypes;
   encKeys_ = encKeys;
   keyRank_ = keyRank;
   if (accepting_) {
      startAccepting();
   }
}

void BaseDealerSettlementDialog::setWallet(const std::shared_ptr<bs::hd::Wallet> &wallet)
{
   widgetPassword()->hide();
   connect(widgetWalletKeys(), &WalletKeysSubmitWidget::keyChanged, [this] { validateGUI(); });

   rootWallet_ = wallet;
   if (signContainer_ && !signContainer_->isOffline()) {
      infoReqId_ = signContainer_->GetInfo(wallet);
   }
}

void BaseDealerSettlementDialog::readyToAccept()
{
   accepting_ = true;
   if (walletInfoReceived_) {
      startAccepting();
   }
}

void BaseDealerSettlementDialog::startAccepting()
{
   if (!rootWallet_) {
      logger_->error("[BaseDealerSettlementDialog::startAccepting] no root wallet");
      return;
   }
   widgetWalletKeys()->init(rootWallet_->getWalletId(), keyRank_, encTypes_, encKeys_);
   widgetWalletKeys()->setFocus();
   QCoreApplication::processEvents();
   adjustSize();
}
