#ifndef __XBT_SETTLEMENT_TRANSACTION_WIDGET_H__
#define __XBT_SETTLEMENT_TRANSACTION_WIDGET_H__

#include <QWidget>
#include <QTimer>

#include <memory>
#include <atomic>

#include "AuthAddress.h"
#include "CommonTypes.h"

namespace Ui {
    class XBTSettlementTransactionWidget;
}
namespace spdlog {
   class logger;
}
class ApplicationSettings;
class CelerClient;
class ReqXBTSettlementContainer;


class XBTSettlementTransactionWidget : public QWidget
{
Q_OBJECT

public:
   XBTSettlementTransactionWidget(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<CelerClient> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ReqXBTSettlementContainer> &
      , QWidget* parent = nullptr);
   ~XBTSettlementTransactionWidget() override = default;

private:
   void populateDetails();
   void onCancel();
   void onAccept();

   void populateXBTDetails();

private slots:
   void onStop();
   void onRetry();
   void updateAcceptButton();
   void onTimerExpired();
   void onTimerTick(int msCurrent, int msDuration);

   void onError(QString);
   void onInfo(QString);

   void onDealerVerificationStateChanged(AddressVerificationState);
   void onAuthWalletInfoReceived();

private:
   std::unique_ptr<Ui::XBTSettlementTransactionWidget> ui_;

   const QString  sValid;
   const QString  sInvalid;
   const QString  sFailed;

   std::shared_ptr<spdlog::logger>              logger_;
   std::shared_ptr<ApplicationSettings>         appSettings_;
   std::shared_ptr<ReqXBTSettlementContainer>   settlContainer_;
};

#endif // __XBT_SETTLEMENT_TRANSACTION_WIDGET_H__
