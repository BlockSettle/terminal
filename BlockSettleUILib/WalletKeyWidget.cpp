#include "WalletKeyWidget.h"
#include "ui_WalletKeyWidget.h"
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <spdlog/spdlog.h>


WalletKeyWidget::WalletKeyWidget(const std::string &walletId, int index, bool password, QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeyWidget())
   , walletId_(walletId), index_(index), password_(password)
   , frejaSign_(spdlog::get(""), 1)
{
   ui_->setupUi(this);
   ui_->radioButtonPassword->setChecked(password);
   ui_->radioButtonFreja->setChecked(!password);
   ui_->pushButtonFreja->setEnabled(false);
   ui_->progressBar->hide();
   onTypeChanged();

   connect(ui_->radioButtonPassword, &QRadioButton::clicked, this, &WalletKeyWidget::onTypeChanged);
   connect(ui_->radioButtonFreja, &QRadioButton::clicked, this, &WalletKeyWidget::onTypeChanged);
   connect(ui_->lineEditPassword, &QLineEdit::textChanged, this, &WalletKeyWidget::onPasswordChanged);
   connect(ui_->lineEditPasswordConfirm, &QLineEdit::textChanged, this, &WalletKeyWidget::onPasswordChanged);
   connect(ui_->comboBoxFrejaId, &QComboBox::editTextChanged, this, &WalletKeyWidget::onFrejaIdChanged);
   connect(ui_->comboBoxFrejaId, SIGNAL(currentIndexChanged(QString)), this, SLOT(onFrejaIdChanged(QString)));
   connect(ui_->pushButtonFreja, &QPushButton::clicked, this, &WalletKeyWidget::onFrejaSignClicked);

   connect(&frejaSign_, &FrejaSignWallet::succeeded, this, &WalletKeyWidget::onFrejaSucceeded);
   connect(&frejaSign_, &FrejaSign::failed, this, &WalletKeyWidget::onFrejaFailed);
   connect(&frejaSign_, &FrejaSign::statusUpdated, this, &WalletKeyWidget::onFrejaStatusUpdated);

   timer_.setInterval(500);
   connect(&timer_, &QTimer::timeout, this, &WalletKeyWidget::onTimer);
}

void WalletKeyWidget::onTypeChanged()
{
   adjustSize();
   if (ui_->radioButtonPassword->isChecked()) {
      ui_->widgetFrejaLayout->hide();
      ui_->widgetPassword->show();
      ui_->widgetPasswordConfirm->show();
   }
   else {
      ui_->widgetFrejaLayout->show();
      ui_->widgetPassword->hide();
      ui_->widgetPasswordConfirm->hide();
   }

   if (password_ != ui_->radioButtonPassword->isChecked()) {
      password_ = ui_->radioButtonPassword->isChecked();
      stop();
      emit keyTypeChanged(index_, password_);
   }
}

void WalletKeyWidget::onPasswordChanged()
{
   ui_->labelPassword->setEnabled(false);
   if ((ui_->lineEditPassword->text() == ui_->lineEditPasswordConfirm->text()) || !ui_->widgetPasswordConfirm->isVisible()) {
      if (!ui_->lineEditPassword->text().isEmpty()) {
         ui_->labelPassword->setEnabled(true);
      }
      emit keyChanged(index_, ui_->lineEditPassword->text().toStdString());
   }
   else {
      emit keyChanged(index_, {});
   }
}

void WalletKeyWidget::onFrejaIdChanged(const QString &text)
{
   emit encKeyChanged(index_, text.toStdString());
   ui_->pushButtonFreja->setEnabled(!text.isNull());
}

void WalletKeyWidget::onFrejaSignClicked()
{
   if (frejaRunning_) {
      cancel();
      ui_->pushButtonFreja->setText(tr("Sign with Freja"));
      return;
   }
   timeLeft_ = 120;
   ui_->progressBar->setMaximum(timeLeft_ * 100);
   ui_->progressBar->show();
   adjustSize();
   timer_.start();
   frejaRunning_ = true;
   frejaSign_.start(ui_->comboBoxFrejaId->currentText(), tr("Activate Freja eID signing"), walletId_);
   ui_->pushButtonFreja->setText(tr("Cancel Freja request"));
   ui_->comboBoxFrejaId->setEnabled(false);
}

void WalletKeyWidget::onFrejaSucceeded(SecureBinaryData password)
{
   stop();
   ui_->pushButtonFreja->setText(tr("Successfully signed"));
   ui_->pushButtonFreja->setEnabled(false);
   emit keyChanged(index_, password);
}

void WalletKeyWidget::onFrejaFailed(const QString &text)
{
   stop();
   ui_->pushButtonFreja->setEnabled(true);
   ui_->pushButtonFreja->setText(tr("Freja failed: %1 - retry").arg(text));
}

void WalletKeyWidget::onFrejaStatusUpdated(const QString &)
{}

void WalletKeyWidget::onTimer()
{
   timeLeft_ -= 0.5;
   if (timeLeft_ <= 0) {
      cancel();
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
   frejaRunning_ = false;
   timer_.stop();
   ui_->progressBar->hide();
   ui_->comboBoxFrejaId->setEnabled(true);
   adjustSize();
}

void WalletKeyWidget::cancel()
{
   if (!password_) {
      frejaSign_.stop(true);
      stop();
   }
}

void WalletKeyWidget::start()
{
   if (!password_ && !frejaRunning_ && !ui_->comboBoxFrejaId->currentText().isEmpty()) {
      onFrejaSignClicked();
      ui_->pushButtonFreja->setEnabled(true);
   }
}

void WalletKeyWidget::setEncryptionKeys(const std::vector<SecureBinaryData> &encKeys, int index)
{
   if (password_) {
      ui_->widgetPasswordConfirm->hide();
   }
   if (encKeys.empty()) {
      return;
   }
   ui_->comboBoxFrejaId->setEditable(false);
   for (const auto &encKey : encKeys) {
      ui_->comboBoxFrejaId->addItem(QString::fromStdString(encKey.toBinStr()));
   }
   if ((index >= 0) && (index < encKeys.size())) {
      ui_->comboBoxFrejaId->setCurrentIndex(index);
   }
}

void WalletKeyWidget::setFixedType(bool on)
{
   if (ui_->comboBoxFrejaId->count() > 0) {
      ui_->comboBoxFrejaId->setEnabled(!on);
   }
   ui_->radioButtonPassword->setVisible(!on);
   ui_->radioButtonFreja->setVisible(!on);
}

void WalletKeyWidget::setFocus()
{
   if (password_) {
      ui_->lineEditPassword->setFocus();
   }
   else {
      ui_->comboBoxFrejaId->setFocus();
   }
}
