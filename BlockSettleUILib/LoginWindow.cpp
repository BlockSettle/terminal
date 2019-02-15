#include "LoginWindow.h"
#include "ui_LoginWindow.h"

#include <QSettings>
#include <QIcon>

#include "AboutDialog.h"
#include "ApplicationSettings.h"
#include "UiUtils.h"
#include "BSMessageBox.h"
#include <spdlog/spdlog.h>
#include "AutheIDClient.h"

namespace {
   int kAuthTimeout = 60;
}

LoginWindow::LoginWindow(const std::shared_ptr<ApplicationSettings> &settings
                         , const std::shared_ptr<spdlog::logger> &logger
                         , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::LoginWindow())
   , settings_(settings)
   , logger_(logger)
{
   ui_->setupUi(this);
   ui_->progressBar->setMaximum(kAuthTimeout * 2); // update every 0.5 sec
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

   autheIDConnection_ = std::make_shared<AutheIDClient>(logger_, settings_->GetAuthKeys());
   connect(autheIDConnection_.get(), &AutheIDClient::authSuccess, this, &LoginWindow::onAutheIDDone);
   connect(autheIDConnection_.get(), &AutheIDClient::failed, this, &LoginWindow::onAutheIDFailed);

   const BinaryData serverPubKey = settings->get<std::string>(ApplicationSettings::authServerPubKey);
   const auto serverHost = settings->get<std::string>(ApplicationSettings::authServerHost);
   const auto serverPort = settings->get<std::string>(ApplicationSettings::authServerPort);
   try {
      autheIDConnection_->connect(serverPubKey, serverHost, serverPort);
   }
   catch (const std::exception &e) {
      logger_->error("[LoginWindow] Failed to establish Auth eID connection: {}", e.what());
   }

   connect(ui_->signWithEidButton, &QPushButton::clicked, this, &LoginWindow::accept);

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &LoginWindow::onTimer);
}

LoginWindow::~LoginWindow() = default;

void LoginWindow::onTimer()
{
   timeLeft_ -= 0.5;
   if (timeLeft_ <= 0) {
      onAutheIDFailed(tr("Timeout"));
   }
   else {
      ui_->progressBar->setValue(timeLeft_ * 2);
      ui_->labelTimeLeft->setText(tr("%1 seconds left").arg((int)timeLeft_));
      ui_->progressBar->repaint();
   }
}

void LoginWindow::setupLoginPage()
{
   timer_.stop();
   state_ = Login;
   timeLeft_ = kAuthTimeout;
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

void LoginWindow::accept()
{
   onAuthPressed();
   ui_->signWithEidButton->setEnabled(false);
}

void LoginWindow::onAuthPressed()
{
   if (state_ == Login) {
      if (autheIDConnection_->authenticate(ui_->lineEditUsername->text().toStdString(), settings_)) {
         setupLoginPage();
         timer_.start();
      }
      else {
         onAutheIDFailed(tr("Auth eID username was rejected"));
         autheIDConnection_->cancel();
      }
      setupCancelPage();
   }
   else {
      setupLoginPage();
      autheIDConnection_->cancel();
   }

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

void LoginWindow::onAutheIDDone(const std::string& jwt)
{
   jwt_= jwt;
   QDialog::accept();
}

void LoginWindow::onAutheIDFailed(const QString &text)
{
   setupLoginPage();
   BSMessageBox loginErrorBox(BSMessageBox::critical, tr("Login failed"), tr("Login failed"), text, this);
   loginErrorBox.exec();
}
