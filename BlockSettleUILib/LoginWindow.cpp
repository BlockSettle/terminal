#include "LoginWindow.h"
#include "ui_LoginWindow.h"
#include "AboutDialog.h"
#include "ApplicationSettings.h"
#include "UiUtils.h"

#include <QSettings>
#include <QIcon>
#include <spdlog/spdlog.h>

LoginWindow::LoginWindow(const std::shared_ptr<ApplicationSettings> &settings, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::LoginWindow())
   , settings_(settings)
   , frejaAuth_(spdlog::get(""))
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

   connect(ui_->pushButtonFreja, &QPushButton::clicked, this, &LoginWindow::onFrejaPressed);
   connect(&frejaAuth_, &FrejaAuth::succeeded, this, &LoginWindow::onFrejaSucceeded);
   connect(&frejaAuth_, &FrejaAuth::failed, this, &LoginWindow::onFrejaFailed);
   connect(&frejaAuth_, &FrejaAuth::statusUpdated, this, &LoginWindow::onFrejaStatusUpdated);
}

void LoginWindow::onTextChanged()
{
   ui_->pushButtonLogin->setEnabled(!(ui_->lineEditPassword->text().isEmpty() || ui_->lineEditUsername->text().isEmpty()));
   ui_->pushButtonFreja->setEnabled(!ui_->lineEditUsername->text().isEmpty());
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

void LoginWindow::onFrejaPressed()
{
   ui_->pushButtonFreja->setEnabled(false);
   if (!frejaAuth_.start(ui_->lineEditUsername->text().toLower())) {
      ui_->pushButtonFreja->setEnabled(true);
   }
}

void LoginWindow::onFrejaSucceeded(const QString &userId, const QString &details)
{
   auto palette = ui_->pushButtonFreja->palette();
   palette.setColor(QPalette::Button, QColor(Qt::green));
   ui_->pushButtonFreja->setAutoFillBackground(true);
   ui_->pushButtonFreja->setPalette(palette);
   ui_->pushButtonFreja->update();
   ui_->pushButtonFreja->setText(tr("Successfully authenticated"));
}
void LoginWindow::onFrejaFailed(const QString &userId, const QString &text)
{
   auto palette = ui_->pushButtonFreja->palette();
   palette.setColor(QPalette::Button, QColor(Qt::red));
   ui_->pushButtonFreja->setAutoFillBackground(true);
   ui_->pushButtonFreja->setPalette(palette);
   ui_->pushButtonFreja->update();
   ui_->pushButtonFreja->setText(tr("Freja auth failed: %1").arg(text));
}

void LoginWindow::onFrejaStatusUpdated(const QString &userId, const QString &status)
{
   ui_->pushButtonFreja->setText(status);
}
