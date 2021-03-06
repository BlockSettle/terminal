/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SettlementContainer.h"
#include "UiUtils.h"

using namespace bs;
using namespace bs::sync;

namespace {

   const auto kUtxoReleaseDelay = std::chrono::seconds(15);

} // namespace

SettlementContainer::SettlementContainer(UtxoReservationToken utxoRes
   , std::unique_ptr<bs::hd::Purpose> walletPurpose, bool expandTxDialogInfo)
   : QObject(nullptr)
   , utxoRes_(std::move(utxoRes))
   , walletPurpose_(std::move(walletPurpose))
   , expandTxDialogInfo_(expandTxDialogInfo)
{}

SettlementContainer::~SettlementContainer()
{
   if (utxoRes_.isValid()) {
      QTimer::singleShot(kUtxoReleaseDelay, [utxoRes = std::move(utxoRes_)] () mutable {
         utxoRes.release();
      });
   }
}

sync::PasswordDialogData SettlementContainer::toPasswordDialogData(QDateTime timestamp) const
{
   bs::sync::PasswordDialogData info;

   info.setValue(PasswordDialogData::SettlementId, id());
   info.setValue(PasswordDialogData::DurationLeft, durationMs());
   info.setValue(PasswordDialogData::DurationTotal, (int)kWaitTimeoutInSec * 1000);

   // Set timestamp that will be used by auth eid server to update timers.
   info.setValue(PasswordDialogData::DurationTimestamp, static_cast<int>(timestamp.toSecsSinceEpoch()));

   info.setValue(PasswordDialogData::ProductGroup, tr(bs::network::Asset::toString(assetType())));
   info.setValue(PasswordDialogData::Security, security());
   info.setValue(PasswordDialogData::Product, product());
   info.setValue(PasswordDialogData::Side, tr(bs::network::Side::toString(side())));
   info.setValue(PasswordDialogData::ExpandTxInfo, expandTxDialogInfo_);

   return info;
}

sync::PasswordDialogData SettlementContainer::toPayOutTxDetailsPasswordDialogData(core::wallet::TXSignRequest payOutReq
   , QDateTime timestamp) const
{
   bs::sync::PasswordDialogData dialogData = toPasswordDialogData(timestamp);

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

void SettlementContainer::releaseUtxoRes()
{
   utxoRes_.release();
}
