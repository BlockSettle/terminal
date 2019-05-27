#include "WalletKeyWidget.h"

#include "ui_WalletKeyWidget.h"
#include <QComboBox>
#include <QGraphicsColorizeEffect>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "AutheIDClient.h"
#include "BSMessageBox.h"

using namespace bs::wallet;
using namespace bs::hd;

namespace
{
const int kAnimationDurationMs = 500;
const QColor kSuccessColor = Qt::green;
const QColor kFailColor = Qt::red;
}

WalletKeyWidget::WalletKeyWidget(AutheIDClient::RequestType requestType
                                       , const bs::hd::WalletInfo &walletInfo
                                       , int keyIndex
                                       , const std::shared_ptr<spdlog::logger> &logger
                                       , const std::shared_ptr<ApplicationSettings> &appSettings
                                       , const std::shared_ptr<ConnectionManager> &connectionManager
                                       , QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeyWidget())
   , requestType_(requestType)
   , walletInfo_(walletInfo)
   , keyIndex_(keyIndex)
   , logger_(logger)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)

{
   ui_->setupUi(this);

   passwordData_.encType = EncryptionType::Unencrypted;
   for (int i = 0; i < walletInfo.encKeys().size(); ++i) {
      const auto &encKey = walletInfo.encKeys().at(i);
      auto deviceInfo = AutheIDClient::getDeviceInfo(encKey.toStdString());

      // Auth eID need to know all devices
      if (!deviceInfo.deviceId.empty()) {
         knownDeviceIds_.push_back(deviceInfo.deviceId);
      }

      if (keyIndex == i) {
         ui_->comboBoxAuthId->addItem(QString::fromStdString(deviceInfo.userId));

         if (deviceInfo.userId.empty()) {
            passwordData_.encType = EncryptionType::Password;
         }
         else {
            passwordData_.encType = EncryptionType::Auth;
         }
      }
   }
   if ((keyIndex >= 0) && (keyIndex < walletInfo.encKeys().size())) {
      ui_->comboBoxAuthId->setCurrentIndex(keyIndex);
      ui_->labelAuthId->setText(ui_->comboBoxAuthId->currentText());
   }

   ui_->radioButtonPassword->setChecked(passwordData_.encType == EncryptionType::Password);
   ui_->radioButtonAuth->setChecked(passwordData_.encType == EncryptionType::Auth);

   ui_->progressBar->hide();
   ui_->progressBar->setValue(0);

   onTypeChanged();

   connect(ui_->radioButtonPassword, &QRadioButton::clicked, this, &WalletKeyWidget::onTypeChanged);
   connect(ui_->radioButtonAuth, &QRadioButton::clicked, this, &WalletKeyWidget::onTypeChanged);
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &WalletKeyWidget::onPasswordChanged);
   connect(ui_->lineEditPasswordConfirm, &QLineEdit::textChanged, this, &WalletKeyWidget::onPasswordChanged);
   connect(ui_->comboBoxAuthId, &QComboBox::editTextChanged, this, &WalletKeyWidget::onAuthIdChanged);
   connect(ui_->comboBoxAuthId, SIGNAL(currentIndexChanged(QString)), this, SLOT(onAuthIdChanged(QString)));
   connect(ui_->pushButtonAuth, &QPushButton::clicked, this, &WalletKeyWidget::onAuthSignClicked);

   connect(ui_->lineEditPassword, &QLineEdit::returnPressed, this, [this](){
      if (ui_->lineEditPasswordConfirm->isVisible()) {
         ui_->lineEditPasswordConfirm->setFocus();
      }
      else {
         emit returnPressed(keyIndex_);
      }
   });

   connect(ui_->lineEditPasswordConfirm, &QLineEdit::returnPressed, this, [this](){
      emit returnPressed(keyIndex_);
   });

   if (ui_->comboBoxAuthId->lineEdit()) {
      connect(ui_->comboBoxAuthId->lineEdit(), &QLineEdit::returnPressed, this, [this](){
         emit returnPressed(keyIndex_);
      });
   }

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &WalletKeyWidget::onTimer);
}

WalletKeyWidget::~WalletKeyWidget() = default;

void WalletKeyWidget::onTypeChanged()
{
   if (ui_->radioButtonPassword->isChecked()) {
      passwordData_.encType = EncryptionType::Password;
      passwordData_.encKey.clear();
      ui_->stackedWidget->setCurrentWidget(ui_->pagePassword);
      ui_->pageEid->setMaximumHeight(0);
      ui_->pagePassword->setMaximumHeight(SHRT_MAX);
   }
   else {
      passwordData_.encType = EncryptionType::Auth;
      ui_->stackedWidget->setCurrentWidget(ui_->pageEid);
      ui_->pageEid->setMaximumHeight(SHRT_MAX);
      ui_->pagePassword->setMaximumHeight(0);

      // optiopnally show eid info box
      if ((useType_ == UseType::ChangeAuthForDialog || useType_ == UseType::ChangeAuthInParent ) && !authNoticeWasShown_) {
         if (MessageBoxAuthNotice(this).exec() == QDialog::Accepted) {
            authNoticeWasShown_ = true;
         }
      }
   }
   emit passwordDataChanged(keyIndex_, passwordData_);
}

void WalletKeyWidget::onPasswordChanged()
{
   ui_->labelPassword->setEnabled(false);
   if ((ui_->lineEditPassword->text() == ui_->lineEditPasswordConfirm->text()) || !ui_->lineEditPasswordConfirm->isVisible()) {
      if (!ui_->lineEditPassword->text().isEmpty()) {
         ui_->labelPassword->setEnabled(true);
      }
      passwordData_.password = ui_->lineEditPassword->text().toStdString();
      emit passwordDataChanged(keyIndex_, passwordData_);
   }
   else {
      emit passwordDataChanged(keyIndex_, PasswordData());
   }

   QString msg;
   bool bGreen = false;
   if (ui_->lineEditPasswordConfirm->isVisible()) {
      auto pwd = ui_->lineEditPassword->text();
      auto pwdCf = ui_->lineEditPasswordConfirm->text();
      if (!pwd.isEmpty() && !pwdCf.isEmpty()) {
         if (pwd == pwdCf) {
            if (pwd.length() < 6) {
               msg = tr("Passwords match but are too short!");
            }
            else {
               msg = tr("Passwords match!");
               bGreen = true;
            }
         }
         else if (pwd.length() < pwdCf.length()) {
            msg = tr("Confirmation Password is too long!");
         }
         else {
            msg = tr("Passwords do not match!");
         }
      }
   }
   ui_->labelPasswordInfo->setStyleSheet(tr("QLabel { color : %1; }").arg(bGreen ? tr("#38C673") : tr("#EE2249")));
   ui_->labelPasswordInfo->setText(msg);
}

void WalletKeyWidget::onAuthIdChanged(const QString &text)
{
   ui_->labelAuthId->setText(text);
   passwordData_.encKey = text.toStdString();
   emit passwordDataChanged(keyIndex_, passwordData_);
   ui_->pushButtonAuth->setEnabled(!text.isEmpty());
}

void WalletKeyWidget::onAuthSignClicked()
{
   if (authRunning_) {
      cancel();
      ui_->pushButtonAuth->setText(tr("Sign with Auth"));
      return;
   }
   else {
      autheIDClient_ = new AutheIDClient(logger_, appSettings_, connectionManager_, this);

      connect(autheIDClient_, &AutheIDClient::succeeded, this, &WalletKeyWidget::onAuthSucceeded);
      connect(autheIDClient_, &AutheIDClient::failed, this, &WalletKeyWidget::onAuthFailed);
   }
   timeLeft_ = 120;
   ui_->progressBar->setMaximum(timeLeft_ * 2);
   ui_->progressBar->show();

   timer_.start();
   authRunning_ = true;

   autheIDClient_->start(requestType_, ui_->comboBoxAuthId->currentText().toStdString()
      , walletInfo_.rootId().toStdString(), knownDeviceIds_);
   ui_->pushButtonAuth->setText(tr("Cancel"));
   ui_->comboBoxAuthId->setEnabled(false);

   //ui_->widgetAuthLayout->hide();
}

void WalletKeyWidget::onAuthSucceeded(const std::string &encKey, const SecureBinaryData &password)
{
   stop();
   ui_->pushButtonAuth->setText(tr("Successfully signed"));
   ui_->pushButtonAuth->setEnabled(false);
   //ui_->widgetAuthLayout->show();
   ui_->labelProgress->setMaximumHeight(0);

   QPropertyAnimation *a = startAuthAnimation(true);
   connect(a, &QPropertyAnimation::finished, [this, encKey, password]() {
      passwordData_.encKey = encKey;
      passwordData_.password = password;
      emit passwordDataChanged(keyIndex_, passwordData_);
   });
}

void WalletKeyWidget::onAuthFailed(QNetworkReply::NetworkError error, AutheIDClient::ErrorType authError)
{
   stop();
   ui_->pushButtonAuth->setEnabled(true);
   ui_->pushButtonAuth->setText(tr("Auth failed: %1 - retry").arg(AutheIDClient::errorString(authError)));
   //ui_->widgetAuthLayout->show();
   
   QPropertyAnimation *a = startAuthAnimation(false);
   connect(a, &QPropertyAnimation::finished, [this]() {
      emit failed();
   });
}

void WalletKeyWidget::onAuthStatusUpdated(const QString &)
{}

void WalletKeyWidget::onTimer()
{
   timeLeft_ -= 0.5;
   if (timeLeft_ <= 0) {
      onAuthFailed(QNetworkReply::NoError, AutheIDClient::ErrorType::Timeout);
   }
   else {
      //ui_->progressBar->setFormat(tr("%1 seconds left").arg((int)timeLeft_));
      ui_->progressBar->setValue(timeLeft_ * 2);
      ui_->labelProgress->setText(tr("%1 seconds left").arg((int)timeLeft_));
      ui_->progressBar->repaint();
   }
}

void WalletKeyWidget::stop()
{
   ui_->lineEditPassword->clear();
   ui_->lineEditPasswordConfirm->clear();
   authRunning_ = false;
   if (autheIDClient_) {
      autheIDClient_->deleteLater();
      autheIDClient_ = nullptr;
   }
   timer_.stop();
   ui_->progressBar->hide();
   ui_->comboBoxAuthId->setEnabled(true);
   //ui_->widgetAuthLayout->show();
}

void WalletKeyWidget::cancel()
{
   if (passwordData_.encType == EncryptionType::Auth) {
      if (autheIDClient_) {
         autheIDClient_->cancel();
      }
      stop();
   }
}

void WalletKeyWidget::start()
{
   if (passwordData_.encType == EncryptionType::Auth && !authRunning_ && !ui_->comboBoxAuthId->currentText().isEmpty()) {
      onAuthSignClicked();
      ui_->progressBar->setValue(0);
   }
}

void WalletKeyWidget::setFocus()
{
   if (passwordData_.encType == EncryptionType::Password) {
      ui_->lineEditPassword->setFocus();
   }
   else {
      ui_->comboBoxAuthId->setFocus();
   }
}

void WalletKeyWidget::setUseType(WalletKeyWidget::UseType useType)
{
   useType_ = useType;

   bool changeAuthType = (useType == UseType::ChangeAuthInParent || useType == UseType::ChangeAuthForDialog);
   bool requestAuthType = (useType == UseType::RequestAuthInParent
                           || useType == UseType::RequestAuthAsDialog
                           || useType == UseType::RequestAuthForDialog);

   ui_->labelPassword->setText(changeAuthType ? tr("New Password") : tr("Password"));
   ui_->comboBoxAuthId->setEditable(changeAuthType);

   ui_->widgetEncTypeSelect->setVisible(changeAuthType);
   ui_->labelPasswordConfirm->setVisible(changeAuthType);
   ui_->lineEditPasswordConfirm->setVisible(changeAuthType);
   ui_->labelPasswordInfo->setVisible(changeAuthType);

   if (requestAuthType) {
      ui_->widgetEncTypeSelect->setMaximumHeight(0);
   }

   ///

   if (useType == UseType::RequestAuthAsDialog
       || useType == UseType::ChangeToEidAsDialog
       || useType == UseType::ChangeToPasswordAsDialog) {
      ui_->widgetCombo->setMaximumHeight(0);
      ui_->widgetCombo->setMaximumWidth(0);
      ui_->progressBar->setMaximumHeight(0);

      ui_->widgetAuthLayout->hide();
      ui_->widgetAuthLayout->setMaximumHeight(0);
   }

   if (useType == UseType::RequestAuthInParent
       || useType == UseType::ChangeAuthInParent) {
//      ui_->widgetAuthIdText->setMaximumHeight(0);
//      ui_->widgetAuthIdText->hide();
//      ui_->widgetAuthIdText->deleteLater();
      ui_->progressBar->setMaximumHeight(0);
      ui_->progressBar->hide();
      //ui_->labelProgress->setMaximumHeight(0);

      ui_->labelAuthIdEmailText->setMinimumWidth(130);
      ui_->labelAuthIdEmailText->setMaximumWidth(SHRT_MAX);
      ui_->labelAuthId->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

      ui_->widgetCombo->setMaximumHeight(0);
      ui_->widgetCombo->setMaximumWidth(0);
      ui_->progressBar->setMaximumHeight(0);
   }

   if (useType == UseType::ChangeAuthForDialog) {
      ui_->pushButtonAuth->hide();

      ui_->progressBar->setMaximumHeight(0);
      ui_->labelProgress->setMaximumHeight(0);
      ui_->widgetAuthIdText->setMaximumHeight(0);
      ui_->widgetAuthIdText->hide();
      ui_->widgetAuthIdText->deleteLater();
      ui_->comboBoxAuthId->clear();
      ui_->radioButtonPassword->click();

      ui_->widgetAuthLayout->hide();
      ui_->widgetAuthLayout->setMaximumHeight(0);
   }

   if (useType == UseType::RequestAuthForDialog) {
      ui_->widgetCombo->setMaximumHeight(0);
      ui_->widgetCombo->setMaximumWidth(0);
      ui_->progressBar->setMaximumHeight(0);
      ui_->labelAuthIdEmailText->setMinimumWidth(130);
      ui_->labelAuthIdEmailText->setMaximumWidth(SHRT_MAX);
      ui_->labelAuthId->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

      ui_->widgetAuthLayout->hide();
      ui_->widgetAuthLayout->setMaximumHeight(0);
   }

   if (useType == UseType::RequestAuthAsDialog && walletInfo_.isPasswordOnly()) {
      ui_->labelPassword->setMaximumWidth(70);
      ui_->labelPassword->setMinimumWidth(70);
   }

   if (requestAuthType && walletInfo_.encType() == EncryptionType::Password) {
      setMaximumHeight(40);
   }
}

QPropertyAnimation* WalletKeyWidget::startAuthAnimation(bool success)
{
   QGraphicsColorizeEffect *eff = new QGraphicsColorizeEffect(this);
   eff->setColor(success ? kSuccessColor : kFailColor);
   ui_->labelAuthId->setGraphicsEffect(eff);

   QPropertyAnimation *a = new QPropertyAnimation(eff, "strength");
   a->setDuration(kAnimationDurationMs);
   a->setStartValue(0.0);
   a->setEndValue(1.0);
   a->setEasingCurve(QEasingCurve::Linear);
   a->start(QPropertyAnimation::DeleteWhenStopped);

   return a;
}
