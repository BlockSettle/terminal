#include "BaseDealerSettlementDialog.h"
#include "SettlementContainer.h"
#include <spdlog/spdlog.h>


BaseDealerSettlementDialog::BaseDealerSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::SettlementContainer> &settlContainer
      , QWidget* parent)
   : QDialog(parent)
   , logger_(logger)
   , settlContainer_(settlContainer)
{
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStarted, this, &BaseDealerSettlementDialog::onTimerStarted);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerStopped, this, &BaseDealerSettlementDialog::onTimerStopped);
   connect(settlContainer_.get(), &bs::SettlementContainer::timerTick, this, &BaseDealerSettlementDialog::onTimerTick);

   connect(settlContainer_.get(), &bs::SettlementContainer::error, [this](QString msg) { setCriticalHintMessage(msg); });
   connect(settlContainer_.get(), &bs::SettlementContainer::info, [this](QString msg) { setHintText(msg); });
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

   const auto formatString = tr("<b><span style=\"color: red;\">%1</span></b>");
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
