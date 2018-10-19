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
   , int index, bool password, QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeyWidget())
   , walletId_(walletId), index_(index)
   , password_(password)
//   , authSign_(spdlog::get(""), 1)
//   , prompt_(prompt)
   , mobileClient_(new MobileClient(spdlog::get(""), this))
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

//   connect(&authSign_, &AuthSignWallet::succeeded, this, &WalletKeyWidget::onAuthSucceeded);
//   connect(&authSign_, &AuthSignWallet::failed, this, &WalletKeyWidget::onAuthFailed);
//   connect(&authSign_, &AuthSignWallet::statusUpdated, this, &WalletKeyWidget::onAuthStatusUpdated);

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

   ui_->labelFrejeId->setVisible(!password_ && showAuthId_);
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
}

void WalletKeyWidget::onAuthIdChanged(const QString &text)
{
   ui_->labelFrejeId->setText(text);
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
   ui_->progressBar->show();
   timer_.start();
   authRunning_ = true;
//   authSign_.start(ui_->comboBoxAuthId->currentText(),
//      prompt_.isEmpty() ? tr("Activate Auth eID signing") : prompt_, walletId_);
   mobileClient_->start(requestType_, ui_->comboBoxAuthId->currentText().toStdString(), walletId_);
   ui_->pushButtonAuth->setText(tr("Cancel Auth request"));
   ui_->comboBoxAuthId->setEnabled(false);

   if (hideAuthControlsOnSignClicked_) {
      ui_->widgetAuthLayout->hide();
   }
}

void WalletKeyWidget::onAuthSucceeded(const std::string &deviceId, const SecureBinaryData &password)
{
   deviceId_ = deviceId;

   stop();
   ui_->pushButtonAuth->setText(tr("Successfully signed"));
   ui_->pushButtonAuth->setEnabled(false);
   ui_->widgetAuthLayout->show();

   QPropertyAnimation *a = startAuthAnimation(true);
   connect(a, &QPropertyAnimation::finished, [this, password]() {
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
//      authSign_.stop(true);
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
      //authSign_.stop(true);
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
   for (const auto &encKey : encKeys) {
      ui_->comboBoxAuthId->addItem(QString::fromStdString(encKey.toBinStr()));
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

void WalletKeyWidget::setHideAuthControlsOnSignClicked(bool value)
{
   hideAuthControlsOnSignClicked_ = value;
}

const string &WalletKeyWidget::deviceId() const
{
   return deviceId_;
}

QPropertyAnimation* WalletKeyWidget::startAuthAnimation(bool success)
{
   QGraphicsColorizeEffect *eff = new QGraphicsColorizeEffect(this);
   eff->setColor(success ? kSuccessColor : kFailColor);
   ui_->labelFrejeId->setGraphicsEffect(eff);

   QPropertyAnimation *a = new QPropertyAnimation(eff, "strength");
   a->setDuration(kAnimationDurationMs);
   a->setStartValue(0.0);
   a->setEndValue(1.0);
   a->setEasingCurve(QEasingCurve::Linear);
   a->start(QPropertyAnimation::DeleteWhenStopped);

   return a;
}
