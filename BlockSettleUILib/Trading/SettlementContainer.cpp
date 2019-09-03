#include "SettlementContainer.h"
#include "UiUtils.h"

using namespace bs;

SettlementContainer::SettlementContainer()
   : QObject(nullptr)
{}

sync::PasswordDialogData SettlementContainer::toPasswordDialogData() const
{
   bs::sync::PasswordDialogData info;

   info.setValue("SettlementId", QString::fromStdString(id()));
   info.setValue("Duration", durationMs());

   info.setValue("ProductGroup", tr(bs::network::Asset::toString(assetType())));
   info.setValue("Security", QString::fromStdString(security()));
   info.setValue("Product", QString::fromStdString(product()));
   info.setValue("Side", tr(bs::network::Side::toString(side())));

   return info;
}

sync::PasswordDialogData SettlementContainer::toPayOutTxDetailsPasswordDialogData(core::wallet::TXSignRequest payOutReq) const
{
   bs::sync::PasswordDialogData dialogData = toPasswordDialogData();

   dialogData.setValue("Title", tr("Settlement Pay-Out"));
   dialogData.setValue("Duration", 30000);

   // tx details
   dialogData.setValue("InputAmount", QStringLiteral("- %2 %1")
                 .arg(UiUtils::XbtCurrency)
                 .arg(UiUtils::displayAmount(payOutReq.inputAmount())));

   dialogData.setValue("ReturnAmount", QStringLiteral("- %2 %1")
                 .arg(UiUtils::XbtCurrency)
                 .arg(UiUtils::displayAmount(payOutReq.change.value)));

   return dialogData;
}

void SettlementContainer::startTimer(const unsigned int durationSeconds)
{
   timer_.stop();
   timer_.setInterval(250);
   msDuration_ = durationSeconds * 1000;
   startTime_ = std::chrono::steady_clock::now();

   connect(&timer_, &QTimer::timeout, [this] {
      const auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime_);
      msTimeLeft_ = msDuration_ - timeDiff.count();
      if (msTimeLeft_ < 0) {
         timer_.stop();
         msDuration_ = 0;
         msTimeLeft_ = 0;
         emit timerExpired();
      }
      else {
         emit timerTick(msTimeLeft_, msDuration_);
      }
   });
   timer_.start();
   emit timerStarted(msDuration_);
}

void SettlementContainer::stopTimer()
{
   msDuration_ = 0;
   msTimeLeft_ = 0;
   timer_.stop();
   emit timerStopped();
}
