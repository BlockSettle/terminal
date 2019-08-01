#ifndef __SETTLEMENT_CONTAINER_H__
#define __SETTLEMENT_CONTAINER_H__

#include <chrono>
#include <string>
#include <QObject>
#include <QTimer>
#include "ArmoryConnection.h"
#include "CommonTypes.h"
#include "EncryptionUtils.h"
#include "PasswordDialogData.h"

namespace bs {

   class SettlementContainer : public QObject
   {
      Q_OBJECT
   public:
      explicit SettlementContainer();
      ~SettlementContainer() override = default;

      virtual bool cancel() = 0;
      virtual bool isAcceptable() const = 0;

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

   signals:
      void error(QString);
      void info(QString);

      void readyToAccept();
      void readyToActivate();
      void completed();
      void failed();

      void timerExpired();
      void timerTick(int msCurrent, int msDuration);
      void timerStarted(int msDuration);
      void timerStopped();

   protected slots:
      void startTimer(const unsigned int durationSeconds);
      void stopTimer();

   private:
      QTimer   timer_;
      int      msDuration_ = 0;
      int      msTimeLeft_ = 0;
      std::chrono::steady_clock::time_point  startTime_;
   };

}  // namespace bs

#endif // __SETTLEMENT_CONTAINER_H__
