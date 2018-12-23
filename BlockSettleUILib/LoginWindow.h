#ifndef __LOGIN_WINDOW_H__
#define __LOGIN_WINDOW_H__

#include <QDialog>
#include <memory>

namespace Ui {
    class LoginWindow;
};

class ApplicationSettings;

class LoginWindow : public QDialog
{
Q_OBJECT

public:
   LoginWindow(const std::shared_ptr<ApplicationSettings> &, QWidget* parent = nullptr);
   ~LoginWindow() override;

   QString getUsername() const;
   QString getPassword() const;

   bool isAutheID() const { return autheID_; }

private slots:
   void onLoginPressed();
   void onTextChanged();
   void onAuthPressed();

   void onAuthSucceeded(const QString &userId, const QString &details);
   void onAuthFailed(const QString &userId, const QString &text);
   void onAuthStatusUpdated(const QString &userId, const QString &status);

private:
   std::unique_ptr<Ui::LoginWindow> ui_;
   std::shared_ptr<ApplicationSettings> settings_;
   bool autheID_;
};

#endif // __LOGIN_WINDOW_H__
