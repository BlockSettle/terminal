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
   LoginWindow(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<BsClientQt> &bsClient
      , std::shared_ptr<ApplicationSettings> &settings
      , QWidget* parent = nullptr);
   ~LoginWindow() override;

   enum State {
      Idle,
      WaitLoginResult,
   };

   QString email() const;
   BsClientLoginResult *result() const { return result_.get(); }

private slots:
   void onStartLoginDone(bool success, const std::string &errorMsg);
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
