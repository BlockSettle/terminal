#include "BaseDealerSettlementDialog.h"
#include <QCoreApplication>
#include "Wallets/SyncHDWallet.h"
#include "SettlementContainer.h"
#include "SignContainer.h"
#include <spdlog/spdlog.h>


BaseDealerSettlementDialog::BaseDealerSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::SettlementContainer> &settlContainer
      , const std::shared_ptr<SignContainer> &signContainer
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , QWidget* parent)
   : QDialog(parent)
   , logger_(logger)
   , settlContainer_(settlContainer)
   , signContainer_(signContainer)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
{
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStarted, this, &BaseDealerSettlementDialog::onTimerStarted);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStopped, this, &BaseDealerSettlementDialog::onTimerStopped);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerTick, this, &BaseDealerSettlementDialog::onTimerTick);
   connect(settlContainer_.get(), &bs::SettlementContainer::completed, this, &QDialog::accept);

   connect(settlContainer_.get(), &bs::SettlementContainer::error, [this](QString msg) { setCriticalHintMessage(msg); });
   connect(settlContainer_.get(), &bs::SettlementContainer::info, [this](QString msg) { setHintText(msg); });

   connect(signContainer_.get(), &SignContainer::QWalletInfo, this, &BaseDealerSettlementDialog::onWalletInfo);
}

void BaseDealerSettlementDialog::connectToProgressBar(QProgressBar *progressBar, QLabel *timeLeftLabel)
{
   progressBar_ = progressBar;
   progressBar_->hide();

   timeLeftLabel_ = timeLeftLabel;
   timeLeftLabel_->hide();
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

void BaseDealerSettlementDialog::setAuthPasswordPrompt(const QString &prompt)
{
   authPrompt_ = prompt;
}

void BaseDealerSettlementDialog::onTimerStarted(int msDuration)
{
   timeLeftLabel_->show();
   timeLeftLabel_->setText(tr("%1 second(s) remaining")
                               .arg(QString::number(msDuration > 0 ? msDuration/1000 : 0)));

   progressBar_->show();
   progressBar_->setMaximum(msDuration);
   progressBar_->setMinimum(0);
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
   //widgetWalletKeys()->cancel();
   settlContainer_->cancel();
   QDialog::reject();
}

void BaseDealerSettlementDialog::onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo &walletInfo)
{
   if (!infoReqId_ || (reqId != infoReqId_)) {
      return;
   }
   infoReqId_ = 0;
   walletInfoReceived_ = true;

   // just update walletInfo_  to save walletName and id
   walletInfo_.setEncKeys(walletInfo.encKeys());
   walletInfo_.setEncTypes(walletInfo.encTypes());
   walletInfo_.setKeyRank(walletInfo.keyRank());

   if (accepting_) {
      startAccepting();
   }
}

void BaseDealerSettlementDialog::setWallet(const std::shared_ptr<bs::sync::hd::Wallet> &wallet)
{
   widgetPassword()->hide();
   //connect(widgetWalletKeys(), &WalletKeysSubmitWidget::keyChanged, [this] { validateGUI(); });

   rootWallet_ = wallet;
   if (signContainer_ && !signContainer_->isOffline()) {
      infoReqId_ = signContainer_->GetInfo(rootWallet_->walletId());
   }
   walletInfo_ = bs::hd::WalletInfo(nullptr, rootWallet_);
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
//   widgetWalletKeys()->init(AutheIDClient::SettlementTransaction, walletInfo_
//                            , WalletKeyWidget::UseType::RequestAuthInParent, logger_, appSettings_, connectionManager_);
//   widgetPassword()->show();
//   widgetWalletKeys()->setFocus();
   QCoreApplication::processEvents();
   adjustSize();
}
