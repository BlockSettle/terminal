#ifndef __LOGIN_WINDOW_H__
#define __LOGIN_WINDOW_H__

#include <QTimer>
#include <QDialog>
#include <memory>

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
      , std::shared_ptr<ApplicationSettings> &settings
      , QWidget* parent = nullptr);
   ~LoginWindow() override;

   enum State {
      Login,
      Cancel
   };

   QString getUsername() const;

   std::shared_ptr<BsClient> client() { return client_; }

signals:
   void startLogin(const QString &login);
   void cancelLogin();

public slots:
   void onStartLoginDone(bool success);

private slots:
   void onTextChanged();
   void onAuthPressed();

   void onAuthStatusUpdated(const QString &userId, const QString &status);

   void onTimer();

   void setupLoginPage();
   void setupCancelPage();

protected:
   void accept() override;

private:
   std::unique_ptr<Ui::LoginWindow>       ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   settings_;
   std::shared_ptr<BsClient>              client_;

   State       state_ = State::Login;
   QTimer      timer_;
   float       timeLeft_;
};

#endif // __LOGIN_WINDOW_H__
