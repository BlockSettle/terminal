/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "LoginWindow.h"

#include <spdlog/spdlog.h>
#include <QIcon>
#include "InfoDialogs/AboutDialog.h"
#include "ApplicationSettings.h"
#include "BSMessageBox.h"
#include "BsClient.h"
#include "UiUtils.h"
#include "ZmqContext.h"
#include "ui_LoginWindow.h"
#include "FutureValue.h"
#include "QBitmap"

namespace {
   const auto kAutheIdTimeout = int(BsClient::autheidLoginTimeout() / std::chrono::seconds(1));
   const auto kProdTitle = QObject::tr("Login to BlockSettle");
   const auto kTestTitle = QObject::tr("Test Environment Login");

   const auto kCreateAccountProd = QObject::tr("Get a BlockSettle Account");
   const auto kCreateAccountTest = QObject::tr("Create your Test Account");

   //<span style = " font-size:12px;">Not signed up yet ? < / span><br><a href = "{GetAccountLink}"><span style = " font-size:12px; text-decoration: underline; color:#fefeff">Get a BlockSettle Account< / span>< / a>
}

LoginWindow::LoginWindow(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<BsClientQt> &bsClient
   , std::shared_ptr<ApplicationSettings> &settings
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::LoginWindow())
   , logger_(logger)
   , settings_(settings)
   , bsClient_(bsClient)
{
   ui_->setupUi(this);
   ui_->progressBar->setMaximum(kAutheIdTimeout * 2); // update every 0.5 sec
   const auto version = ui_->loginVersionLabel->text().replace(QLatin1String("{Version}")
      , tr("Version %1").arg(QString::fromStdString(AboutDialog::version())));
   ui_->loginVersionLabel->setText(version);
   resize(minimumSize());

   const bool isProd = settings_->get<int>(ApplicationSettings::envConfiguration) ==
      static_cast<int>(ApplicationSettings::EnvConfiguration::Production);

   ApplicationSettings::Setting urlType;
   auto getAccountText = ui_->labelGetAccount->text();
   QString title;
   if (isProd) {
      urlType = ApplicationSettings::GetAccount_UrlProd;
      title = kProdTitle;
   }
   else {
      urlType = ApplicationSettings::GetAccount_UrlTest;
      title = kTestTitle;
      getAccountText.replace(kCreateAccountProd, kCreateAccountTest);
   }

   ui_->widgetSignup->setProperty("prodEnv", QVariant(true));
   getAccountText.replace(QLatin1String("{GetAccountLink}")
      , settings->get<QString>(urlType));
   ui_->labelGetAccount->setText(getAccountText);
   ui_->widgetSignup->update();

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
   timer_.start();

   updateState();

   connect(bsClient_.get(), &BsClientQt::startLoginDone, this, &LoginWindow::onStartLoginDone);
   connect(bsClient_.get(), &BsClientQt::getLoginResultDone, this, &LoginWindow::onGetLoginResultDone);
}

LoginWindow::LoginWindow(const std::shared_ptr<spdlog::logger>& logger
   , ApplicationSettings::EnvConfiguration envCfg, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::LoginWindow())
   , logger_(logger)
{
   ui_->setupUi(this);
   ui_->progressBar->setMaximum(kAutheIdTimeout * 2); // update every 0.5 sec
   const auto version = ui_->loginVersionLabel->text().replace(QLatin1String("{Version}")
      , tr("Version %1").arg(QString::fromStdString(AboutDialog::version())));
   ui_->loginVersionLabel->setText(version);
   resize(minimumSize());

   const bool isProd = (envCfg == ApplicationSettings::EnvConfiguration::Production);

   ApplicationSettings::Setting urlType;
   auto getAccountText = ui_->labelGetAccount->text();
   QString title;
   if (isProd) {
      urlType = ApplicationSettings::GetAccount_UrlProd;
      title = kProdTitle;
   } else {
      urlType = ApplicationSettings::GetAccount_UrlTest;
      title = kTestTitle;
      getAccountText.replace(kCreateAccountProd, kCreateAccountTest);
   }

   ui_->widgetSignup->setProperty("prodEnv", QVariant(true));
/*   getAccountText.replace(QLatin1String("{GetAccountLink}")
      , settings->get<QString>(urlType));*/  // TODO: use async retrieval
   ui_->labelGetAccount->setText(getAccountText);
   ui_->widgetSignup->update();

   connect(ui_->lineEditUsername, &QLineEdit::textChanged, this, &LoginWindow::onTextChanged);

//   ui_->checkBoxRememberUsername->setChecked(settings_->get<bool>(ApplicationSettings::rememberLoginUserName));

/*   const QString username = settings_->get<QString>(ApplicationSettings::celerUsername);
   if (!username.isEmpty() && ui_->checkBoxRememberUsername->isChecked()) {
      ui_->lineEditUsername->setText(username);
   } else {
      ui_->lineEditUsername->setFocus();
   }*/   //TODO: use async retrieval

   connect(ui_->signWithEidButton, &QPushButton::clicked, this, &LoginWindow::accept);

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &LoginWindow::onTimer);
   timer_.start();

   updateState();
}

LoginWindow::~LoginWindow() = default;

void LoginWindow::onTimer()
{
   if (state_ != WaitLoginResult) {
      return;
   }

   timeLeft_ -= 0.5f;

   if (timeLeft_ < 0) {
      setState(Idle);
      return;
   }

   ui_->progressBar->setValue(int(timeLeft_ * 2));
   ui_->labelTimeLeft->setText(tr("%1 seconds left").arg(int(timeLeft_)));
}

void LoginWindow::onTextChanged()
{
   updateState();
}

QString LoginWindow::email() const
{
   return ui_->lineEditUsername->text().toLower();
}

void LoginWindow::setLogin(const QString& login)
{
   ui_->lineEditUsername->setText(login);
   updateState();
}

void LoginWindow::setRememberLogin(bool flag)
{
   ui_->checkBoxRememberUsername->setChecked(flag);
}

void LoginWindow::onLoginStarted(const std::string& login, bool success
   , const std::string& errMsg)
{
   if (success) {
      setState(WaitLoginResult);
   }
   else {
      setState(Idle);
      displayError(errMsg);
   }
}

void LoginWindow::onLoggedIn(const BsClientLoginResult& result)
{
   if (result.status == AutheIDClient::Cancelled || result.status == AutheIDClient::Timeout) {
      setState(Idle);
      return;
   }
   if (result.status != AutheIDClient::NoError) {
      setState(Idle);
      displayError(result.errorMsg);
      return;
   }
   QDialog::accept();
}

void LoginWindow::onStartLoginDone(bool success, const std::string &errorMsg)
{
   if (!success) {
      setState(Idle);
      displayError(errorMsg);
      return;
   }

   bsClient_->getLoginResult();

   setState(WaitLoginResult);
}

void LoginWindow::onGetLoginResultDone(const BsClientLoginResult &result)
{
   if (result.status == AutheIDClient::Cancelled || result.status == AutheIDClient::Timeout) {
      setState(Idle);
      return;
   }

   if (result.status != AutheIDClient::NoError) {
      setState(Idle);
      displayError(result.errorMsg);
      return;
   }

   result_ = std::make_unique<BsClientLoginResult>(result);
   QDialog::accept();
}

void LoginWindow::accept()
{
   onAuthPressed();
}

void LoginWindow::reject()
{
   if (state_ == WaitLoginResult) {
      if (bsClient_) {
         bsClient_->cancelLogin();
      }
      else {
         emit needCancelLogin();
      }
   }
   QDialog::reject();
}

void LoginWindow::setState(LoginWindow::State state)
{
   if (state != state_) {
      state_ = state;
      updateState();
   }
}

void LoginWindow::updateState()
{
   switch (state_) {
      case Idle:
         ui_->signWithEidButton->setText(tr("Sign in with Auth eID"));
         ui_->stackedWidgetAuth->setCurrentWidget(ui_->pageLogin);
         ui_->progressBar->setValue(0);
         ui_->labelTimeLeft->setText(QStringLiteral(""));
         ui_->signWithEidButton->setEnabled(!ui_->lineEditUsername->text().isEmpty());
         ui_->lineEditUsername->setEnabled(true);
         break;
      case WaitLoginResult:
         ui_->signWithEidButton->setText(tr("Cancel"));
         ui_->signWithEidButton->setEnabled(true);
         ui_->stackedWidgetAuth->setCurrentWidget(ui_->pageCancel);
         ui_->lineEditUsername->setEnabled(false);
         break;
   }
}

void LoginWindow::displayError(const std::string &message)
{
   BSMessageBox loginErrorBox(BSMessageBox::critical, tr("Login failed"), tr("Login failed")
      , QString::fromStdString(message), this);
   loginErrorBox.exec();
}

void LoginWindow::onAuthPressed()
{
   if (state_ != Idle) {
      reject();
      return;
   }

   timeLeft_ = kAutheIdTimeout;

   QString login = ui_->lineEditUsername->text().trimmed();
   ui_->lineEditUsername->setText(login);

   if (bsClient_) {
      bsClient_->startLogin(login.toStdString());
   }
   else {
      emit needStartLogin(login.toStdString());
   }

   if (ui_->checkBoxRememberUsername->isChecked()) {
      if (settings_) {
         settings_->set(ApplicationSettings::rememberLoginUserName, true);
         settings_->set(ApplicationSettings::celerUsername, ui_->lineEditUsername->text());
      }
      else {
         emit putSetting(ApplicationSettings::rememberLoginUserName, true);
         emit putSetting(ApplicationSettings::celerUsername, ui_->lineEditUsername->text());
      }
   }
   else {
      if (settings_) {
         settings_->set(ApplicationSettings::rememberLoginUserName, false);
      }
      else {
         emit putSetting(ApplicationSettings::rememberLoginUserName, false);
      }
   }

   setState(WaitLoginResult);
}
