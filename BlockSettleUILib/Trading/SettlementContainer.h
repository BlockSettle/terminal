/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SETTLEMENT_CONTAINER_H__
#define __SETTLEMENT_CONTAINER_H__

#include <chrono>
#include <string>
#include <QObject>
#include <QTimer>

#include "ArmoryConnection.h"
#include "CommonTypes.h"
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "PasswordDialogData.h"
#include "UtxoReservationToken.h"
#include "ValidityFlag.h"

namespace bs {

   class SettlementContainer : public QObject
   {
      Q_OBJECT
   public:
      explicit SettlementContainer();
      ~SettlementContainer() override;

      virtual bool cancel() = 0;

      virtual void activate() = 0;
      virtual void deactivate() = 0;

      virtual std::string id() const = 0;
      virtual bs::network::Asset::Type assetType() const = 0;
      virtual std::string security() const = 0;
      virtual std::string product() const = 0;
      virtual bs::network::Side::Type side() const = 0;
      virtual double quantity() const = 0;
      virtual double price() const = 0;
      virtual double amount() const = 0;

      int durationMs() const { return msDuration_; }
      int timeLeftMs() const { return msTimeLeft_; }

      virtual bs::sync::PasswordDialogData toPasswordDialogData() const;
      virtual bs::sync::PasswordDialogData toPayOutTxDetailsPasswordDialogData(bs::core::wallet::TXSignRequest payOutReq) const;

      static constexpr unsigned int kWaitTimeoutInSec = 30;
   signals:
      void error(QString);

      void completed();
      void failed();

      void timerExpired();
      void timerStarted(int msDuration);
      void timerStopped();

   protected slots:
      void startTimer(const unsigned int durationSeconds);
      void stopTimer();

   protected:
      void releaseUtxoRes();

      ValidityFlag validityFlag_;
      bs::UtxoReservationToken utxoRes_;

   private:
      QTimer   timer_;
      int      msDuration_ = 0;
      int      msTimeLeft_ = 0;
      std::chrono::steady_clock::time_point startTime_;

   };

}  // namespace bs

#endif // __SETTLEMENT_CONTAINER_H__
