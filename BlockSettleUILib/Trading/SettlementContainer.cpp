#include "SettlementContainer.h"
#include "UiUtils.h"

using namespace bs;
using namespace bs::sync;

SettlementContainer::SettlementContainer()
   : QObject(nullptr)
{}

sync::PasswordDialogData SettlementContainer::toPasswordDialogData() const
{
   bs::sync::PasswordDialogData info;

   info.setValue(PasswordDialogData::SettlementId, id());
   info.setValue(PasswordDialogData::DurationLeft, durationMs());
   info.setValue(PasswordDialogData::DurationTotal, (int)kWaitTimeoutInSec * 1000);

   // Set timestamp that will be used by auth eid server to update timers.
   // TODO: Use time from PB and use it for all counters.
   const int timestamp = static_cast<int>(std::chrono::system_clock::now().time_since_epoch() / std::chrono::seconds(1));
   info.setValue(PasswordDialogData::DurationTimestamp, timestamp);

   info.setValue(PasswordDialogData::ProductGroup, tr(bs::network::Asset::toString(assetType())));
   info.setValue(PasswordDialogData::Security, security());
   info.setValue(PasswordDialogData::Product, product());
   info.setValue(PasswordDialogData::Side, tr(bs::network::Side::toString(side())));

   return info;
}

sync::PasswordDialogData SettlementContainer::toPayOutTxDetailsPasswordDialogData(core::wallet::TXSignRequest payOutReq) const
{
   bs::sync::PasswordDialogData dialogData = toPasswordDialogData();

   dialogData.setValue(PasswordDialogData::Title, tr("Settlement Pay-Out"));
   dialogData.setValue(PasswordDialogData::DurationLeft, static_cast<int>(kWaitTimeoutInSec * 1000));
   dialogData.setValue(PasswordDialogData::DurationTotal, static_cast<int>(kWaitTimeoutInSec * 1000));
   dialogData.setValue(PasswordDialogData::SettlementPayOutVisible, true);

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
