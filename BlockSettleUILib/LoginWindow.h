/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __LOGIN_WINDOW_H__
#define __LOGIN_WINDOW_H__

#include <QTimer>
#include <QDialog>
#include <memory>
#include "ApplicationSettings.h"
#include "AutheIDClient.h"

namespace Ui {
    class LoginWindow;
}
namespace spdlog {
   class logger;
}

struct BsClientLoginResult;
struct NetworkSettings;

class ApplicationSettings;
class BsClientQt;

class LoginWindow : public QDialog
{
Q_OBJECT

public:
   [[deprecated]] LoginWindow(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<BsClientQt> &bsClient
      , std::shared_ptr<ApplicationSettings> &settings
      , QWidget* parent = nullptr);
   LoginWindow(const std::shared_ptr<spdlog::logger>& logger
      , ApplicationSettings::EnvConfiguration, QWidget* parent = nullptr);
   ~LoginWindow() override;

   enum State {
      Idle,
      WaitLoginResult,
   };

   QString email() const;
   [[deprecated]] BsClientLoginResult *result() const { return result_.get(); }

   void setLogin(const QString&);
   void setRememberLogin(bool);
   void onLoginStarted(const std::string& login, bool success, const std::string& errMsg);
   void onLoggedIn(const BsClientLoginResult&);

signals:
   void putSetting(ApplicationSettings::Setting, const QVariant&);
   void needStartLogin(const std::string& login);
   void needCancelLogin();

private slots:
   void onStartLoginDone(bool success, const std::string &errorMsg);
   void onGetLoginResultDone(const BsClientLoginResult &result);  //deprecated
   void onTextChanged();
   void onAuthPressed();
   void onTimer();

protected:
   void accept() override;
   void reject() override;

private:
   void setState(State state);
   void updateState();
   void displayError(const std::string &message);

   std::unique_ptr<Ui::LoginWindow>       ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   settings_;

   State       state_{State::Idle};
   QTimer      timer_;
   float       timeLeft_{};
   std::shared_ptr<BsClientQt>            bsClient_;
   std::unique_ptr<BsClientLoginResult>   result_;
};

#endif // __LOGIN_WINDOW_H__
