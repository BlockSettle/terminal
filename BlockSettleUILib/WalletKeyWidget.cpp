#include "WalletKeyWidget.h"

#include "ui_WalletKeyWidget.h"
#include <QComboBox>
#include <QGraphicsColorizeEffect>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <spdlog/spdlog.h>


namespace {

const int kAnimationDurationMs = 500;

const QColor kSuccessColor = Qt::green;
const QColor kFailColor = Qt::red;

}


WalletKeyWidget::WalletKeyWidget(const std::string &walletId, int index, bool password,
   const QString &prompt, QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::WalletKeyWidget())
   , walletId_(walletId), index_(index), password_(password)
   , frejaSign_(spdlog::get(""), 1)
   , prompt_(prompt)
{
   ui_->setupUi(this);
   ui_->radioButtonPassword->setChecked(password);
   ui_->radioButtonFreja->setChecked(!password);
   ui_->pushButtonFreja->setEnabled(false);
   ui_->progressBar->hide();
   ui_->progressBar->setValue(0);

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

   ui_->labelFrejeId->setVisible(!password_ && showFrejaId_);
   ui_->widgetFrejaLayout->setVisible(!password_);
   
   ui_->pushButtonFreja->setVisible(!hideFrejaConnect_);
   ui_->comboBoxFrejaId->setVisible(!hideFrejaCombobox_);
   ui_->labelFrejaInfo->setVisible(!hideFrejaEmailLabel_ && !password_ && !hideFrejaCombobox_);
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

void WalletKeyWidget::onFrejaIdChanged(const QString &text)
{
   ui_->labelFrejeId->setText(text);
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
   timer_.start();
   frejaRunning_ = true;
   frejaSign_.start(ui_->comboBoxFrejaId->currentText(),
      prompt_.isEmpty() ? tr("Activate Freja eID signing") : prompt_, walletId_);
   ui_->pushButtonFreja->setText(tr("Cancel Freja request"));
   ui_->comboBoxFrejaId->setEnabled(false);

   if (hideFrejaControlsOnSignClicked_) {
      ui_->widgetFrejaLayout->hide();
   }
}

void WalletKeyWidget::onFrejaSucceeded(SecureBinaryData password)
{
   stop();
   ui_->pushButtonFreja->setText(tr("Successfully signed"));
   ui_->pushButtonFreja->setEnabled(false);
   ui_->widgetFrejaLayout->show();

   QPropertyAnimation *a = startFrejaAnimation(true);
   connect(a, &QPropertyAnimation::finished, [this, password]() {
      emit keyChanged(index_, password);
   });
}

void WalletKeyWidget::onFrejaFailed(const QString &text)
{
   
   stop();
   ui_->pushButtonFreja->setEnabled(true);
   ui_->pushButtonFreja->setText(tr("Freja failed: %1 - retry").arg(text));
   ui_->widgetFrejaLayout->show();
   
   QPropertyAnimation *a = startFrejaAnimation(false);
   connect(a, &QPropertyAnimation::finished, [this]() {
      emit failed();
   });
}

void WalletKeyWidget::onFrejaStatusUpdated(const QString &)
{}

void WalletKeyWidget::onTimer()
{
   timeLeft_ -= 0.5;
   if (timeLeft_ <= 0) {
      frejaSign_.stop(true);
      onFrejaFailed(tr("Timeout"));
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
   if (!progressBarFixed_) {
      ui_->progressBar->hide();
   }
   ui_->comboBoxFrejaId->setEnabled(true);
   ui_->widgetFrejaLayout->show();
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
   encryptionKeysSet_ = true;
   if (password_) {
      ui_->labelPasswordConfirm->hide();
      ui_->lineEditPasswordConfirm->hide();
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
   ui_->labelPadding->setVisible(!on);
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

void WalletKeyWidget::setHideFrejaConnect(bool value)
{
   hideFrejaConnect_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setHideFrejaCombobox(bool value)
{
   hideFrejaCombobox_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setProgressBarFixed(bool value)
{
   progressBarFixed_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setShowFrejaId(bool value)
{
   showFrejaId_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setHideFrejaEmailLabel(bool value)
{
   hideFrejaEmailLabel_ = value;
   onTypeChanged();
}

void WalletKeyWidget::setHideFrejaControlsOnSignClicked(bool value)
{
   hideFrejaControlsOnSignClicked_ = value;
}

void WalletKeyWidget::setCreateUsername(const QString& username)
{
   ui_->comboBoxFrejaId->setEditText(username);
}

QPropertyAnimation* WalletKeyWidget::startFrejaAnimation(bool success)
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
