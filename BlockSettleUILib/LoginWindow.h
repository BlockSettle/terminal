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
   LoginWindow(std::shared_ptr<ApplicationSettings> settings, QWidget* parent = nullptr );
   ~LoginWindow() override = default;

public slots:
   void onLoginPressed();
   void onTextChanged();

   QString getUsername() const;
   QString getPassword() const;
private:
   Ui::LoginWindow* ui_;
   std::shared_ptr<ApplicationSettings> settings_;
};

#endif // __LOGIN_WINDOW_H__
