#ifndef __CC_SETTLEMENT_TRANSACTION_WIDGET_H__
#define __CC_SETTLEMENT_TRANSACTION_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include <atomic>

#include "BinaryData.h"
#include "CheckRecipSigner.h"
#include "CommonTypes.h"
#include "MetaData.h"
#include "SettlementWallet.h"
#include "UtxoReservation.h"

namespace Ui {
    class CCSettlementTransactionWidget;
}
namespace spdlog {
   class logger;
}
class ApplicationSettings;
class CelerClient;
class WalletsManager;
class ReqCCSettlementContainer;

namespace SwigClient
{
   class BtcWallet;
};

class CCSettlementTransactionWidget : public QWidget
{
Q_OBJECT

public:
   CCSettlementTransactionWidget(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<CelerClient> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ReqCCSettlementContainer> &
      , QWidget* parent = nullptr);
   ~CCSettlementTransactionWidget() override = default;

   Q_INVOKABLE void cancel();    //TODO: will be vanished

private:
   void onCancel();
   void onAccept();
   void populateDetails();

private slots:
   void onTimerExpired();
   void onTimerTick(int msCurrent, int msDuration);
   void initSigning();
   void updateAcceptButton();
   void onGenAddrVerified(bool, QString);
   void onPaymentVerified(bool, QString);
   void onError(QString);
   void onInfo(QString);

private:
   std::unique_ptr<Ui::CCSettlementTransactionWidget> ui_;

   const QString  sValid;
   const QString  sInvalid;

   std::shared_ptr<spdlog::logger>     logger_;
   const std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ReqCCSettlementContainer>    settlContainer_;
};

#endif // __CC_SETTLEMENT_TRANSACTION_WIDGET_H__
