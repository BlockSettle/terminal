#ifndef __CC_SETTLEMENT_TRANSACTION_WIDGET_H__
#define __CC_SETTLEMENT_TRANSACTION_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>
#include <atomic>

#include "BinaryData.h"
#include "CheckRecipSigner.h"
#include "CommonTypes.h"
#include "UtxoReservation.h"

// TODO: Obsoleted, delete file after Sign Settlement moved to Signer

namespace Ui {
    class CCSettlementTransactionWidget;
}
namespace spdlog {
   class logger;
}
class ApplicationSettings;
class BaseCelerClient;
class WalletsManager;
class ReqCCSettlementContainer;
class ConnectionManager;

namespace SwigClient
{
   class BtcWallet;
}

class CCSettlementTransactionWidget : public QWidget
{
Q_OBJECT

public:
   CCSettlementTransactionWidget(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<BaseCelerClient> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ReqCCSettlementContainer> &
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , QWidget* parent = nullptr);
   ~CCSettlementTransactionWidget() noexcept override;

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
   void onKeyChanged();

private:
   std::unique_ptr<Ui::CCSettlementTransactionWidget> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   const std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ReqCCSettlementContainer>    settlContainer_;
   std::shared_ptr<ConnectionManager>           connectionManager_;

   const QString  sValid_;
   const QString  sInvalid_;
};

#endif // __CC_SETTLEMENT_TRANSACTION_WIDGET_H__
