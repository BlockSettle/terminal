#ifndef __BASE_DEALER_SETTLEMENT_DIALOG_H__
#define __BASE_DEALER_SETTLEMENT_DIALOG_H__

#include <QDateTime>
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QWidget>

#include <chrono>
#include <memory>

#include "QWalletInfo.h"

// TODO: Obsoleted, delete file after Sign Settlement moved to Signer

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
   }
   class SettlementContainer;
}
class SignContainer;
class ApplicationSettings;
class ConnectionManager;

class BaseDealerSettlementDialog : public QDialog
{
Q_OBJECT

public:
   BaseDealerSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::SettlementContainer> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ConnectionManager> &
      , QWidget* parent = nullptr);
   ~BaseDealerSettlementDialog() noexcept override = default;

   BaseDealerSettlementDialog(const BaseDealerSettlementDialog&) = delete;
   BaseDealerSettlementDialog& operator = (const BaseDealerSettlementDialog&) = delete;

   BaseDealerSettlementDialog(BaseDealerSettlementDialog&&) = delete;
   BaseDealerSettlementDialog& operator = (BaseDealerSettlementDialog&&) = delete;

protected slots:
   void onTimerTick(int msCurrent, int msDuration);
   void onTimerStarted(int msDuration);
   void onTimerStopped();

   void onWalletInfo(unsigned int reqId, const bs::hd::WalletInfo& walletInfo);

protected:
   void reject() override;

   void setWallet(const std::shared_ptr<bs::sync::hd::Wallet> &);

   virtual void readyToAccept();
   void startAccepting();

   void connectToProgressBar(QProgressBar *progressBar, QLabel *timeLeftLabel);
   void connectToHintLabel(QLabel *hintLabel, QLabel *errorLabel);

   void setHintText(const QString& hint);
   void setCriticalHintMessage(const QString& hint);

   bool isMessageCritical() const { return hintSetToCritical_; }

   virtual QWidget *widgetPassword() const = 0;
   //virtual WalletKeysSubmitWidget *widgetWalletKeys() const = 0;
   virtual QLabel *labelHint() const = 0;
   virtual QLabel *labelPassword() const = 0;

   virtual void validateGUI() = 0;

   void setAuthPasswordPrompt(const QString &prompt);

protected:
   std::shared_ptr<spdlog::logger>  logger_;

private:
   std::shared_ptr<bs::SettlementContainer>  settlContainer_;
   std::shared_ptr<SignContainer>            signContainer_;
   std::shared_ptr<bs::sync::hd::Wallet>     rootWallet_;
   QProgressBar   *progressBar_ = nullptr;
   QLabel         *timeLeftLabel_ = nullptr;
   QLabel         *hintLabel_ = nullptr;
   QLabel         *errorLabel_ = nullptr;
   bool           hintSetToCritical_ = false;
   unsigned int   infoReqId_ = 0;
   bool           walletInfoReceived_ = false;
   bool           accepting_ = false;
   QString        authPrompt_;
   const std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager> connectionManager_;
   bs::hd::WalletInfo walletInfo_;
};

#endif // __BASE_DEALER_SETTLEMENT_DIALOG_H__
