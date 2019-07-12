#ifndef __LOGIN_WINDOW_H__
#define __LOGIN_WINDOW_H__

#include <QTimer>
#include <QDialog>
#include <memory>
#include "AutheIDClient.h"

namespace Ui {
    class LoginWindow;
}
namespace spdlog {
   class logger;
}

class ApplicationSettings;
class BsClient;

class LoginWindow : public QDialog
{
Q_OBJECT

public:
   LoginWindow(const std::shared_ptr<spdlog::logger> &logger
      , std::shared_ptr<ApplicationSettings> &settings, BsClient *client
      , QWidget* parent = nullptr);
   ~LoginWindow() override;

   enum State {
      Login,
      Cancel
   };

   QString getUsername() const;

private slots:
   void onStartLoginDone(AutheIDClient::ErrorType errorCode);
   void onGetLoginResultDone(AutheIDClient::ErrorType errorCode);
   void onTextChanged();
   void onAuthPressed();
   void onAuthStatusUpdated(const QString &userId, const QString &status);
   void onTimer();

protected:
   void accept() override;
   void reject() override;

private:
   void setupLoginPage();
   void setupCancelPage();

   std::unique_ptr<Ui::LoginWindow>       ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   settings_;

   State       state_ = State::Login;
   QTimer      timer_;
   float       timeLeft_{};
   BsClient    *bsClient_{};
};

#endif // __LOGIN_WINDOW_H__
