#ifndef __LOGIN_WINDOW_H__
#define __LOGIN_WINDOW_H__

#include <QTimer>
#include <QDialog>
#include <memory>
#include "AutheIDClient.h"
#include "ZMQ_BIP15X_Helpers.h"

namespace Ui {
    class LoginWindow;
}
namespace spdlog {
   class logger;
}

struct BsClientLoginResult;
struct NetworkSettings;

class ApplicationSettings;
class NetworkSettingsLoader;
class BsClient;

class LoginWindow : public QDialog
{
Q_OBJECT

public:
   LoginWindow(const std::shared_ptr<spdlog::logger> &logger
      , std::shared_ptr<ApplicationSettings> &settings
      , const ZmqBipNewKeyCb &cbApprove
      , QWidget* parent = nullptr);
   ~LoginWindow() override;

   enum State {
      Idle,
      WaitNetworkSettings,
      WaitLoginResult,
   };

   QString email() const;
   BsClientLoginResult *result() const { return result_.get(); }
   std::unique_ptr<BsClient> getClient();
   const NetworkSettings &networkSettings() const;

private slots:
   void onStartLoginDone(AutheIDClient::ErrorType errorCode);
   void onGetLoginResultDone(const BsClientLoginResult &result);
   void onTextChanged();
   void onAuthPressed();
   void onTimer();

protected:
   void accept() override;
   void reject() override;

private:
   void setState(State state);
   void updateState();

   std::unique_ptr<Ui::LoginWindow>       ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   settings_;
   ZmqBipNewKeyCb                         cbApprove_;

   State       state_{State::Idle};
   QTimer      timer_;
   float       timeLeft_{};
   std::unique_ptr<BsClient> bsClient_;
   std::unique_ptr<NetworkSettingsLoader> networkSettingsLoader_;
   std::unique_ptr<BsClientLoginResult> result_;
};

#endif // __LOGIN_WINDOW_H__
