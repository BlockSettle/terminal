#ifndef __LOGIN_WINDOW_H__
#define __LOGIN_WINDOW_H__

#include <QDialog>
#include <memory>
#include "FrejaREST.h"

namespace Ui {
    class LoginWindow;
};

class ApplicationSettings;

class LoginWindow : public QDialog
{
Q_OBJECT

public:
   LoginWindow(const std::shared_ptr<ApplicationSettings> &, QWidget* parent = nullptr);
   ~LoginWindow() override = default;

   QString getUsername() const;
   QString getPassword() const;

private slots:
   void onLoginPressed();
   void onTextChanged();
   void onFrejaPressed();

   void onFrejaSucceeded(const QString &userId, const QString &details);
   void onFrejaFailed(const QString &userId, const QString &text);
   void onFrejaStatusUpdated(const QString &userId, const QString &status);

private:
   Ui::LoginWindow* ui_;
   std::shared_ptr<ApplicationSettings> settings_;
   FrejaAuth   frejaAuth_;
};

#endif // __LOGIN_WINDOW_H__
