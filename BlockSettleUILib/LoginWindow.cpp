#include "LoginWindow.h"

#include <spdlog/spdlog.h>
#include <QIcon>
#include "AboutDialog.h"
#include "ApplicationSettings.h"
#include "BsClient.h"
#include "BSMessageBox.h"
#include "UiUtils.h"
#include "ui_LoginWindow.h"

namespace {
   const auto AuthTimeout = int(BsClient::getDefaultAutheidAuthTimeout() / std::chrono::seconds(1));
}

LoginWindow::LoginWindow(const std::shared_ptr<spdlog::logger> &logger
   , std::shared_ptr<ApplicationSettings> &settings
   , BsClient *client
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::LoginWindow())
   , logger_(logger)
   , settings_(settings)
   , bsClient_(client)
{
   ui_->setupUi(this);
   ui_->progressBar->setMaximum(AuthTimeout * 2); // update every 0.5 sec
   const auto version = ui_->loginVersionLabel->text().replace(QLatin1String("{Version}")
      , tr("Version %1").arg(QString::fromStdString(AboutDialog::version())));
   ui_->loginVersionLabel->setText(version);
   resize(minimumSize());

   const auto accountLink = ui_->labelGetAccount->text().replace(QLatin1String("{GetAccountLink}")
      , settings->get<QString>(ApplicationSettings::GetAccount_Url));
   ui_->labelGetAccount->setText(accountLink);

   connect(ui_->lineEditUsername, &QLineEdit::textChanged, this, &LoginWindow::onTextChanged);

   ui_->checkBoxRememberUsername->setChecked(settings_->get<bool>(ApplicationSettings::rememberLoginUserName));

   const QString username = settings_->get<QString>(ApplicationSettings::celerUsername);
   if (!username.isEmpty() && ui_->checkBoxRememberUsername->isChecked()) {
      ui_->lineEditUsername->setText(username);
   }
   else {
      ui_->lineEditUsername->setFocus();
   }

   connect(ui_->signWithEidButton, &QPushButton::clicked, this, &LoginWindow::accept);

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &LoginWindow::onTimer);

   connect(bsClient_, &BsClient::startLoginDone, this, &LoginWindow::onStartLoginDone);
   connect(bsClient_, &BsClient::getLoginResultDone, this, &LoginWindow::onGetLoginResultDone);

   onTextChanged();
}

LoginWindow::~LoginWindow() = default;

void LoginWindow::onTimer()
{
   timeLeft_ -= 0.5f;
   if (timeLeft_ <= 0) {
      //onAutheIDFailed(tr("Timeout"));
      setupLoginPage();
   } else {
      ui_->progressBar->setValue(int(timeLeft_ * 2));
      ui_->labelTimeLeft->setText(tr("%1 seconds left").arg(int(timeLeft_)));
   }
}

void LoginWindow::setupLoginPage()
{
   timer_.stop();
   state_ = Login;
   timeLeft_ = AuthTimeout;
   ui_->signWithEidButton->setText(tr("Sign in with Auth eID"));
   ui_->stackedWidgetAuth->setCurrentWidget(ui_->pageLogin);
   ui_->progressBar->setValue(0);
   ui_->labelTimeLeft->setText(QStringLiteral(""));
}

void LoginWindow::setupCancelPage()
{
   state_ = Cancel;
   ui_->signWithEidButton->setText(tr("Cancel"));
   ui_->stackedWidgetAuth->setCurrentWidget(ui_->pageCancel);
}

void LoginWindow::onTextChanged()
{
   ui_->signWithEidButton->setEnabled(!ui_->lineEditUsername->text().isEmpty());
}

QString LoginWindow::getUsername() const
{
   return ui_->lineEditUsername->text().toLower();
}

void LoginWindow::onStartLoginDone(AutheIDClient::ErrorType errorCode)
{
   if (errorCode != AutheIDClient::NoError) {
      setupLoginPage();
      BSMessageBox loginErrorBox(BSMessageBox::critical, tr("Login failed"), tr("Login failed")
         , AutheIDClient::errorString(errorCode), this);
      loginErrorBox.exec();
      QDialog::reject();
      return;
   }

   timer_.start();
   bsClient_->getLoginResult();
}

void LoginWindow::onGetLoginResultDone(AutheIDClient::ErrorType errorCode)
{
   if (errorCode == AutheIDClient::Cancelled) {
      setupLoginPage();
      return;
   }

   if (errorCode != AutheIDClient::NoError) {
      setupLoginPage();
      BSMessageBox loginErrorBox(BSMessageBox::critical, tr("Login failed"), tr("Login failed")
         , AutheIDClient::errorString(errorCode), this);
      loginErrorBox.exec();
      QDialog::reject();
      return;
   }

   QDialog::accept();
}

void LoginWindow::accept()
{
   onAuthPressed();
}

void LoginWindow::reject()
{
   if (state_ == Cancel) {
      bsClient_->cancelLogin();
   }
   QDialog::reject();
}

void LoginWindow::onAuthPressed()
{
   if (state_ == Cancel) {
      reject();
      return;
   }

   QString login = ui_->lineEditUsername->text().trimmed();
   ui_->lineEditUsername->setText(login);

   setupLoginPage();
   setupCancelPage();
   bsClient_->startLogin(login.toStdString());

   if (ui_->checkBoxRememberUsername->isChecked()) {
      settings_->set(ApplicationSettings::rememberLoginUserName, true);
      settings_->set(ApplicationSettings::celerUsername, ui_->lineEditUsername->text());
   }
   else {
      settings_->set(ApplicationSettings::rememberLoginUserName, false);
   }
}

void LoginWindow::onAuthStatusUpdated(const QString &userId, const QString &status)
{
   ui_->signWithEidButton->setText(status);
}
