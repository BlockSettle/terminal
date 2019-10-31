#include "LoginWindow.h"

#include <spdlog/spdlog.h>
#include <QIcon>
#include "AboutDialog.h"
#include "ApplicationSettings.h"
#include "BSMessageBox.h"
#include "BsClient.h"
#include "NetworkSettingsLoader.h"
#include "UiUtils.h"
#include "ZmqContext.h"
#include "ui_LoginWindow.h"
#include "FutureValue.h"

namespace {
   const auto kAutheIdTimeout = int(BsClient::autheidLoginTimeout() / std::chrono::seconds(1));
}

LoginWindow::LoginWindow(const std::shared_ptr<spdlog::logger> &logger
   , std::shared_ptr<ApplicationSettings> &settings
   , const ZmqBipNewKeyCb &cbApprove
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::LoginWindow())
   , logger_(logger)
   , settings_(settings)
   , cbApprove_(cbApprove)
{
   ui_->setupUi(this);
   ui_->progressBar->setMaximum(kAutheIdTimeout * 2); // update every 0.5 sec
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

std::unique_ptr<BsClient> LoginWindow::getClient()
{
   return std::move(bsClient_);
}

const NetworkSettings &LoginWindow::networkSettings() const
{
   return networkSettingsLoader_->settings();
}

void LoginWindow::onStartLoginDone(AutheIDClient::ErrorType errorCode)
{
   if (errorCode != AutheIDClient::NoError) {
      setState(Idle);

      BSMessageBox loginErrorBox(BSMessageBox::critical, tr("Login failed"), tr("Login failed")
         , AutheIDClient::errorString(errorCode), this);
      loginErrorBox.exec();
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

      BSMessageBox loginErrorBox(BSMessageBox::critical, tr("Login failed"), tr("Login failed")
         , AutheIDClient::errorString(result.status), this);
      loginErrorBox.exec();
      return;
   }

   result_ = std::make_unique<BsClientLoginResult>(std::move(result));
   QDialog::accept();
}

void LoginWindow::accept()
{
   onAuthPressed();
}

void LoginWindow::reject()
{
   if (state_ == WaitLoginResult) {
      bsClient_->cancelLogin();
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
      case WaitNetworkSettings:
      case WaitLoginResult:
         ui_->signWithEidButton->setText(tr("Cancel"));
         ui_->signWithEidButton->setEnabled(true);
         ui_->stackedWidgetAuth->setCurrentWidget(ui_->pageCancel);
         ui_->lineEditUsername->setEnabled(false);
         break;
   }
}

void LoginWindow::onAuthPressed()
{
   if (state_ != Idle) {
      reject();
      return;
   }

   timeLeft_ = kAutheIdTimeout;

   networkSettingsLoader_ = std::make_unique<NetworkSettingsLoader>(logger_
      , settings_->pubBridgeHost(), settings_->pubBridgePort(), cbApprove_);

   connect(networkSettingsLoader_.get(), &NetworkSettingsLoader::succeed, this, [this] {
      setState(WaitLoginResult);

      QString login = ui_->lineEditUsername->text().trimmed();
      ui_->lineEditUsername->setText(login);

      BsClientParams params;
      params.connectAddress = networkSettingsLoader_->settings().proxy.host;
      params.connectPort = networkSettingsLoader_->settings().proxy.port;
      params.context = std::make_shared<ZmqContext>(logger_);
      params.newServerKeyCallback = [](const BsClientParams::NewKey &newKey) {
         // FIXME: Show GUI prompt
         newKey.prompt->setValue(true);
      };

      bsClient_ = std::make_unique<BsClient>(logger_, params);
      connect(bsClient_.get(), &BsClient::startLoginDone, this, &LoginWindow::onStartLoginDone);
      connect(bsClient_.get(), &BsClient::getLoginResultDone, this, &LoginWindow::onGetLoginResultDone);

      bsClient_->startLogin(login.toStdString());
   });

   connect(networkSettingsLoader_.get(), &NetworkSettingsLoader::failed, this, [this](const QString &errorMsg) {
      BSMessageBox(BSMessageBox::critical, tr("Network settings"), errorMsg, this).exec();
      setState(Idle);
   });

   networkSettingsLoader_->loadSettings();

   if (ui_->checkBoxRememberUsername->isChecked()) {
      settings_->set(ApplicationSettings::rememberLoginUserName, true);
      settings_->set(ApplicationSettings::celerUsername, ui_->lineEditUsername->text());
   }
   else {
      settings_->set(ApplicationSettings::rememberLoginUserName, false);
   }

   setState(WaitNetworkSettings);
}
