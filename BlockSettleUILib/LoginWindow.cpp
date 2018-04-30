#include "LoginWindow.h"
#include "ui_LoginWindow.h"
#include "AboutDialog.h"
#include "ApplicationSettings.h"
#include "UiUtils.h"

#include <QSettings>
#include <QIcon>

LoginWindow::LoginWindow(std::shared_ptr<ApplicationSettings> settings, QWidget* parent)
 : QDialog(parent)
 , ui_(new Ui::LoginWindow())
 , settings_(settings)
{
   ui_->setupUi(this);
   ui_->loginVersionLabel->setText(tr("Version %1").arg(QString::fromStdString(AboutDialog::version())));
   resize(minimumSize());

   const auto resetLink = ui_->labelResetPassword->text().replace(QLatin1String("{ResetPasswordLink}")
      , settings->get<QString>(ApplicationSettings::ResetPassword_Url));
   ui_->labelResetPassword->setText(resetLink);

   const auto accountLink = ui_->labelGetAccount->text().replace(QLatin1String("{GetAccountLink}")
      , settings->get<QString>(ApplicationSettings::GetAccount_Url));
   ui_->labelGetAccount->setText(accountLink);

   connect(ui_->pushButtonLogin, &QPushButton::clicked, this, &LoginWindow::onLoginPressed);
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &LoginWindow::onTextChanged);
   connect(ui_->lineEditUsername, &QLineEdit::textChanged, this, &LoginWindow::onTextChanged);

   const QString username = settings_->get<QString>(ApplicationSettings::celerUsername);
   if (!username.isEmpty()) {
      ui_->lineEditUsername->setText(username);
      ui_->lineEditPassword->setFocus();
   }
   else {
      ui_->lineEditUsername->setFocus();
   }
}

void LoginWindow::onTextChanged()
{
   ui_->pushButtonLogin->setEnabled(!(ui_->lineEditPassword->text().isEmpty() || ui_->lineEditUsername->text().isEmpty()));
}

void LoginWindow::onLoginPressed()
{
   if (ui_->checkBoxRememberUsername->isChecked()) {
      settings_->set(ApplicationSettings::celerUsername, ui_->lineEditUsername->text());
   }
   accept();
}

QString LoginWindow::getUsername() const
{
   return ui_->lineEditUsername->text().toLower();
}

QString LoginWindow::getPassword() const
{
   return ui_->lineEditPassword->text();
}


