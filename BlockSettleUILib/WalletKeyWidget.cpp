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
#include "MobileClient.h"

namespace
{

const int kAnimationDurationMs = 500;

const QColor kSuccessColor = Qt::green;
const QColor kFailColor = Qt::red;

}


WalletKeyWidget::WalletKeyWidget(MobileClientRequest requestType, const std::string &walletId
   , int index, bool password, const std::pair<autheid::PrivateKey, autheid::PublicKey> &authKeys
   , QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeyWidget())
   , walletId_(walletId), index_(index)
   , password_(password)
//   , prompt_(prompt)
   , mobileClient_(new MobileClient(spdlog::get(""), authKeys, this))
   , requestType_(requestType)
{
   ui_->setupUi(this);
   ui_->radioButtonPassword->setChecked(password);
   ui_->radioButtonAuth->setChecked(!password);
   ui_->pushButtonAuth->setEnabled(false);
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

   connect(mobileClient_, &MobileClient::succeeded, this, &WalletKeyWidget::onAuthSucceeded);
   connect(mobileClient_, &MobileClient::failed, this, &WalletKeyWidget::onAuthFailed);

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &WalletKeyWidget::onTimer);
}

void WalletKeyWidget::init(const std::shared_ptr<ApplicationSettings> &appSettings, const QString& username)
{
   std::string serverPubKey = appSettings->get<std::string>(ApplicationSettings::authServerPubKey);
   std::string serverHost = appSettings->get<std::string>(ApplicationSettings::authServerHost);
   std::string serverPort = appSettings->get<std::string>(ApplicationSettings::authServerPort);

   mobileClient_->init(serverPubKey, serverHost, serverPort);

   ui_->comboBoxAuthId->setEditText(username);
}

WalletKeyWidget::~WalletKeyWidget() = default;

void WalletKeyWidget::onTypeChanged()
{
   if (password_ != ui_->radioButtonPassword->isChecked()) {
      password_ = ui_->radioButtonPassword->isChecked();
      stop();
      emit keyTypeChanged(index_, password_);
   }

   ui_->labelPassword->setVisible(password_);
   ui_->lineEditPassword->setVisible(password_);
   ui_->labelPasswordConfirm->setVisible(password_ && !encryptionKeysSet_);
   ui_->lineEditPasswordConfirm->setVisible(password_ && !encryptionKeysSet_);
   ui_->labelPasswordInfo->setVisible(password_);
   ui_->labelPasswordWarning->setVisible(password_ && !hidePasswordWarning_);

   ui_->labelAuthId->setVisible(!password_ && showAuthId_);
   ui_->widgetAuthLayout->setVisible(!password_);
   
   ui_->pushButtonAuth->setVisible(!hideAuthConnect_);
   ui_->comboBoxAuthId->setVisible(!hideAuthCombobox_);
   ui_->widgetSpacing->setVisible(!progressBarFixed_);
   ui_->labelAuthInfo->setVisible(!hideAuthEmailLabel_ && !password_ && !hideAuthCombobox_);
}

void WalletKeyWidget::onPasswordChanged()
{
   ui_->labelPassword->setEnabled(false);
   if ((ui_->lineEditPassword->text() == ui_->lineEditPasswordConfirm->text()) || !ui_->lineEditPasswordConfirm->isVisible()) {
      if (!ui_->lineEditPassword->text().isEmpty()) {
         ui_->labelPassword->setEnabled(true);
      }
      emit keyChanged(index_, ui_->lineEditPassword->text().toStdString());
   }
   else {
      emit keyChanged(index_, {});
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
   emit encKeyChanged(index_, text.toStdString());
   ui_->pushButtonAuth->setEnabled(!text.isNull());
}

void WalletKeyWidget::onAuthSignClicked()
{
   if (authRunning_) {
      cancel();
      ui_->pushButtonAuth->setText(tr("Sign with Auth"));
      return;
   }
   timeLeft_ = 120;
   ui_->progressBar->setMaximum(timeLeft_ * 100);
   if (!hideProgressBar_) {
      ui_->progressBar->show();
   }
   timer_.start();
   authRunning_ = true;

   mobileClient_->start(requestType_, ui_->comboBoxAuthId->currentText().toStdString()
      , walletId_, knownDeviceIds_);
   ui_->pushButtonAuth->setText(tr("Cancel Auth request"));
   ui_->comboBoxAuthId->setEnabled(false);

   if (hideAuthControlsOnSignClicked_) {
      ui_->widgetAuthLayout->hide();
   }
}

void WalletKeyWidget::onAuthSucceeded(const std::string &encKey, const SecureBinaryData &password)
{
   stop();
   ui_->pushButtonAuth->setText(tr("Successfully signed"));
   ui_->pushButtonAuth->setEnabled(false);
   ui_->widgetAuthLayout->show();

   QPropertyAnimation *a = startAuthAnimation(true);
   connect(a, &QPropertyAnimation::finished, [this, encKey, password]() {
      emit encKeyChanged(index_, encKey);
      emit keyChanged(index_, password);
   });
}

void WalletKeyWidget::onAuthFailed(const QString &text)
{
   
   stop();
   ui_->pushButtonAuth->setEnabled(true);
   ui_->pushButtonAuth->setText(tr("Auth failed: %1 - retry").arg(text));
   ui_->widgetAuthLayout->show();
   
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
      onAuthFailed(tr("Timeout"));
   }
   else {
      ui_->progressBar->setFormat(tr("%1 seconds left").arg((int)timeLeft_));
      ui_->progressBar->setValue(timeLeft_ * 100);
   }
}

void WalletKeyWidget::stop()
{
   ui_->lineEditPassword->clear();
   ui_->lineEditPasswordConfirm->clear();
   authRunning_ = false;
   timer_.stop();
   if (!progressBarFixed_) {
      ui_->progressBar->hide();
   }
   ui_->comboBoxAuthId->setEnabled(true);
   ui_->widgetAuthLayout->show();
}

void WalletKeyWidget::cancel()
{
   if (!password_) {
      mobileClient_->cancel();
      stop();
   }
}

void WalletKeyWidget::start()
{
   if (!password_ && !authRunning_ && !ui_->comboBoxAuthId->currentText().isEmpty()) {
      onAuthSignClicked();
      ui_->progressBar->setValue(0);
      ui_->pushButtonAuth->setEnabled(true);
   }
}

void WalletKeyWidget::setEncryptionKeys(const std::vector<SecureBinaryData> &encKeys, int index)
{
   encryptionKeysSet_ = true;
   if (password_) {
      ui_->labelPasswordConfirm->hide();
      ui_->lineEditPasswordConfirm->hide();
   }
   if (encKeys.empty()) {
      return;
   }
   ui_->comboBoxAuthId->setEditable(false);

   knownDeviceIds_.clear();
   for (const auto &encKey : encKeys) {
      auto deviceInfo = MobileClient::getDeviceInfo(encKey.toBinStr());
      if (!deviceInfo.deviceId.empty()) {
         knownDeviceIds_.push_back(deviceInfo.deviceId);
      }
      ui_->comboBoxAuthId->addItem(QString::fromStdString(deviceInfo.userId));
   }
   if ((index >= 0) && (index < encKeys.size())) {
      ui_->comboBoxAuthId->setCurrentIndex(index);
   }
}

void WalletKeyWidget::setFixedType(bool on)
{
   if (ui_->comboBoxAuthId->count() > 0) {
      //ui_->comboBoxAuthId->setEnabled(!on);
      ui_->comboBoxAuthId->setEditable(!on);
   }
   ui_->radioButtonPassword->setVisible(!on);
   ui_->radioButtonAuth->setVisible(!on);
}

void WalletKeyWidget::setFocus()
{
   if (password_) {
      ui_->lineEditPassword->setFocus();
   }
   else {
      ui_->comboBoxAuthId->setFocus();
   }
}

void WalletKeyWidget::setHideAuthConnect(bool value)
{
   hideAuthConnect_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setHideAuthCombobox(bool value)
{
   hideAuthCombobox_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setProgressBarFixed(bool value)
{
   progressBarFixed_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setShowAuthId(bool value)
{
   showAuthId_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setPasswordLabelAsNew()
{
   ui_->labelPassword->setText(tr("New Password"));
}

void WalletKeyWidget::setPasswordLabelAsOld()
{
   ui_->labelPassword->setText(tr("Old Password"));
}

void WalletKeyWidget::setHideAuthEmailLabel(bool value)
{
   hideAuthEmailLabel_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setHidePasswordWarning(bool value)
{
   hidePasswordWarning_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setHideAuthControlsOnSignClicked(bool value)
{
   hideAuthControlsOnSignClicked_ = value;
}

void WalletKeyWidget::setHideProgressBar(bool value)
{
   hideProgressBar_ = value;
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
