#include "BaseDealerSettlementDialog.h"
#include "HDWallet.h"
#include "SettlementContainer.h"
#include "SignContainer.h"
#include <spdlog/spdlog.h>
#include <QLineEdit>


BaseDealerSettlementDialog::BaseDealerSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::SettlementContainer> &settlContainer, const std::shared_ptr<SignContainer> &signContainer
      , QWidget* parent)
   : QDialog(parent)
   , logger_(logger)
   , settlContainer_(settlContainer)
   , signContainer_(signContainer)
   , frejaSign_(logger, 1)
{
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStarted, this, &BaseDealerSettlementDialog::onTimerStarted);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStopped, this, &BaseDealerSettlementDialog::onTimerStopped);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerTick, this, &BaseDealerSettlementDialog::onTimerTick);
   connect(settlContainer_.get(), &bs::SettlementContainer::completed, this, &QDialog::accept);

   connect(settlContainer_.get(), &bs::SettlementContainer::error, [this](QString msg) { setCriticalHintMessage(msg); });
   connect(settlContainer_.get(), &bs::SettlementContainer::info, [this](QString msg) { setHintText(msg); });

   connect(signContainer_.get(), &SignContainer::HDWalletInfo, this, &BaseDealerSettlementDialog::onHDWalletInfo);

   connect(&frejaSign_, &FrejaSignWallet::succeeded, this, &BaseDealerSettlementDialog::onFrejaSucceeded);
   connect(&frejaSign_, &FrejaSign::failed, this, &BaseDealerSettlementDialog::onFrejaFailed);
   connect(&frejaSign_, &FrejaSign::statusUpdated, this, &BaseDealerSettlementDialog::onFrejaStatusUpdated);
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
   if (walletInfoReceived_ && (encType_ == bs::wallet::EncryptionType::Freja)) {
      frejaSign_.stop(true);
   }
   settlContainer_->cancel();
   QDialog::reject();
}

void BaseDealerSettlementDialog::onHDWalletInfo(unsigned int id, bs::wallet::EncryptionType encType
   , const SecureBinaryData &encKey)
{
   if (!infoReqId_ || (id != infoReqId_)) {
      return;
   }
   infoReqId_ = 0;
   walletInfoReceived_ = true;
   encType_ = encType;
   userId_ = QString::fromStdString(encKey.toBinStr());
   if (accepting_) {
      startAccepting();
   }
}

void BaseDealerSettlementDialog::setWallet(const std::shared_ptr<bs::hd::Wallet> &wallet)
{
   widgetPassword()->hide();
   connect(lineEditPassword(), &QLineEdit::textChanged, [this](const QString &text) { walletPassword_ = text.toStdString(); });

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
   switch (encType_) {
   case bs::wallet::EncryptionType::Unencrypted:
      labelPassword()->hide();
      break;
   case bs::wallet::EncryptionType::Password:
      widgetPassword()->show();
      lineEditPassword()->setEnabled(true);
      break;
   case bs::wallet::EncryptionType::Freja:
      labelPassword()->show();
      setHintText(tr("Freja sign request sent to your mobile device"));
      frejaSign_.start(userId_, tr("Settlement TX for %1 in wallet %2").arg(QString::fromStdString(settlContainer_->security()))
         .arg(QString::fromStdString(rootWallet_->getName())), rootWallet_->getWalletId());  // set 30s timeout (not implemented by Freja, yet)
      break;
   default: break;
   }
}

void BaseDealerSettlementDialog::onFrejaSucceeded(SecureBinaryData password)
{
   labelPassword()->setText(tr("Signed successfully"));
   walletPassword_ = password;
   settlContainer_->accept(walletPassword_);
}

void BaseDealerSettlementDialog::onFrejaFailed(const QString &text)
{
   labelPassword()->setText(tr("Freja failed: %1").arg(text));
   reject();
}

void BaseDealerSettlementDialog::onFrejaStatusUpdated(const QString &status)
{
   labelPassword()->setText(tr("Freja: %1").arg(status));
}
