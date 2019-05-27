#ifndef __LOGIN_WINDOW_H__
#define __LOGIN_WINDOW_H__

#include <QTimer>
#include <QDialog>
#include <memory>
#include <QNetworkReply>
#include "AutheIDClient.h"

class AutheIDClient;

namespace Ui {
    class LoginWindow;
}
namespace spdlog {
   class logger;
}

class ApplicationSettings;
class ConnectionManager;

class LoginWindow : public QDialog
{
Q_OBJECT

public:
   LoginWindow(const std::shared_ptr<spdlog::logger> &logger
               , const std::shared_ptr<ApplicationSettings> &
               , const std::shared_ptr<ConnectionManager> &
               , QWidget* parent = nullptr);
   ~LoginWindow() override;

   enum State {
      Login,
      Cancel
   };

   QString getUsername() const;

   std::string getJwt() const {return jwt_; }

private slots:
   void onTextChanged();
   void onAuthPressed();

   void onAuthStatusUpdated(const QString &userId, const QString &status);

   void onAutheIDDone(const std::string& email);
   void onAutheIDFailed(QNetworkReply::NetworkError error, AutheIDClient::ErrorType authError);
   void onTimer();

   void setupLoginPage();
   void setupCancelPage();

protected:
   void accept() override;

private:
   std::unique_ptr<Ui::LoginWindow>       ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   settings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<AutheIDClient>         autheIDConnection_ {};

   std::string jwt_;
   State       state_ = State::Login;
   QTimer      timer_;
   float       timeLeft_;
};

#endif // __LOGIN_WINDOW_H__
