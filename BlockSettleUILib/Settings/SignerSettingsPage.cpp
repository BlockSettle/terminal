/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QClipboard>

#include "SignerSettingsPage.h"
#include "ui_SignerSettingsPage.h"
#include "ApplicationSettings.h"
#include "BtcUtils.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "SignersManageWidget.h"
#include "HeadlessContainer.h"

SignerSettingsPage::SignerSettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::SignerSettingsPage{}}
{
   ui_->setupUi(this);
   ui_->widgetTwoWayAuth->hide();
   ui_->checkBoxTwoWayAuth->hide();

   //connect(ui_->comboBoxRunMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SignerSettingsPage::runModeChanged);
   connect(ui_->spinBoxAsSpendLimit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SignerSettingsPage::onAsSpendLimitChanged);
   connect(ui_->pushButtonManageSignerKeys, &QPushButton::clicked, this, &SignerSettingsPage::onManageSignerKeys);

   connect(ui_->pushButtonTerminalKeyCopy, &QPushButton::clicked, this, [this](){
      qApp->clipboard()->setText(ui_->labelTerminalKey->text());
      ui_->pushButtonTerminalKeyCopy->setEnabled(false);
      ui_->pushButtonTerminalKeyCopy->setText(tr("Copied"));
      QTimer::singleShot(2000, this, [this](){
         ui_->pushButtonTerminalKeyCopy->setEnabled(true);
         ui_->pushButtonTerminalKeyCopy->setText(tr("Copy"));
      });
   });
   connect(ui_->pushButtonTerminalKeySave, &QPushButton::clicked, this, [this](){
      QString fileName = QFileDialog::getSaveFileName(this
                                   , tr("Save BlockSettleDB Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + QStringLiteral("Terminal_Public_Key.pub")
                                   , tr("Key files (*.pub)"));

      QFile file(fileName);
      if (file.open(QIODevice::WriteOnly)) {
         file.write(ui_->labelTerminalKey->text().toLatin1());
      }
   });
}

SignerSettingsPage::~SignerSettingsPage() = default;

void SignerSettingsPage::display()
{
   if (appSettings_ && signersProvider_) {
      ui_->checkBoxTwoWayAuth->setChecked(appSettings_->get<bool>(ApplicationSettings::twoWaySignerAuth));
      ui_->comboBoxSigner->setCurrentIndex(signersProvider_->indexOfCurrent());

      showLimits(signersProvider_->indexOfCurrent() == 0);
      ui_->labelTerminalKey->setText(QString::fromStdString(signersProvider_->remoteSignerOwnKey().toHexStr()));
   }
   else {
      ui_->checkBoxTwoWayAuth->setChecked(settings_.at(ApplicationSettings::twoWaySignerAuth).toBool());
      ui_->comboBoxSigner->setCurrentIndex(curSignerIdx_);
      showLimits(curSignerIdx_ == 0);
      ui_->labelTerminalKey->setText(QString::fromStdString(ownKey_));
   }
}

void SignerSettingsPage::reset()
{
   const std::vector<ApplicationSettings::Setting> resetList {
      ApplicationSettings::remoteSigners, ApplicationSettings::autoSignSpendLimit
      , ApplicationSettings::twoWaySignerAuth
   };
   if (appSettings_) {
      for (const auto& setting : resetList) {
         appSettings_->reset(setting, false);
      }
      display();
   }
   else {
      emit resetSettings(resetList);
   }
}

void SignerSettingsPage::showHost(bool show)
{
   ui_->labelHost->setVisible(true);
   ui_->labelHost_2->setVisible(true);
   ui_->comboBoxSigner->setVisible(true);
}

void SignerSettingsPage::showLimits(bool show)
{
   ui_->groupBoxAutoSign->setEnabled(show);
   ui_->labelAsSpendLimit->setEnabled(show);
   ui_->spinBoxAsSpendLimit->setEnabled(show);
   onAsSpendLimitChanged(ui_->spinBoxAsSpendLimit->value());
}

void SignerSettingsPage::showSignerKeySettings(bool show)
{
   //ui_->widgetTwoWayAuth->setVisible(show);
   //ui_->checkBoxTwoWayAuth->setVisible(show);
   ui_->widgetSignerKeyComboBox->setVisible(show);
   ui_->SignerDetailsGroupBox->setVisible(show);
}

void SignerSettingsPage::onAsSpendLimitChanged(double value)
{
   if (value > 0) {
      ui_->labelAsSpendLimit->setText(tr("Spend Limit:"));
   }
   else {
      ui_->labelAsSpendLimit->setText(tr("Spend Limit - unlimited"));
   }
}

void SignerSettingsPage::onManageSignerKeys()
{
   // workaround here - wrap widget by QDialog
   // TODO: fix stylesheet to support popup widgets

   QDialog *dlg = new QDialog(this);
   QVBoxLayout *layout = new QVBoxLayout(dlg);
   layout->setContentsMargins(0,0,0,0);
   dlg->setLayout(layout);
   dlg->setWindowTitle(tr("Manage Signer Connection"));

   if (appSettings_ && signersProvider_) {
      signerKeysWidget_ = new SignerKeysWidget(signersProvider_, appSettings_, this);
   }
   else {
      signerKeysWidget_ = new SignerKeysWidget(this);
   }
   dlg->resize(signerKeysWidget_->size());

   layout->addWidget(signerKeysWidget_);

   connect(signerKeysWidget_, &SignerKeysWidget::needClose, dlg, &QDialog::reject);
   connect(dlg, &QDialog::finished, [this](int) {
      signerKeysWidget_->deleteLater();
      signerKeysWidget_ = nullptr;
   });
   signerKeysWidget_->onSignerSettings(signers_, curSignerIdx_);
   dlg->exec();

   emit signersChanged();
}

void SignerSettingsPage::apply()
{
   if (appSettings_) {
      appSettings_->set(ApplicationSettings::twoWaySignerAuth, ui_->checkBoxTwoWayAuth->isChecked());
      signersProvider_->setupSigner(ui_->comboBoxSigner->currentIndex());
   }
   else {
      emit putSetting(ApplicationSettings::twoWaySignerAuth, ui_->checkBoxTwoWayAuth->isChecked());
      emit setSigner(ui_->comboBoxSigner->currentIndex());
   }
}

void SignerSettingsPage::initSettings()
{
   if (signersProvider_) {
      signersModel_ = new SignersModel(signersProvider_, this);
      connect(signersProvider_.get(), &SignersProvider::dataChanged, this, &SignerSettingsPage::display);
   }
   else {
      signersModel_ = new SignersModel(this);
   }
   signersModel_->setSingleColumnMode(true);
   signersModel_->setHighLightSelectedServer(false);
   ui_->comboBoxSigner->setModel(signersModel_);
}

void SignerSettingsPage::init(const ApplicationSettings::State& state)
{
   if (state.find(ApplicationSettings::twoWaySignerAuth) == state.end()) {
      return;  // not our snapshot
   }
   SettingsPage::init(state);
}

void SignerSettingsPage::onSignerSettings(const QList<SignerHost>& signers
   , const std::string& ownKey, int idxCur)
{
   signers_ = signers;
   curSignerIdx_ = idxCur;
   ownKey_ = ownKey;
   if (signersModel_) {
      signersModel_->onSignerSettings(signers, idxCur);
   }
   if (signerKeysWidget_) {
      signerKeysWidget_->onSignerSettings(signers, idxCur);
   }
   display();
}
