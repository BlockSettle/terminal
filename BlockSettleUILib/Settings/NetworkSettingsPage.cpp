/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMetaEnum>
#include <QTimer>

#include "NetworkSettingsPage.h"
#include "ui_NetworkSettingsPage.h"
#include "ApplicationSettings.h"
#include "ArmoryServersWidget.h"
#include "WebSocketClient.h"
#include "HeadlessContainer.h"


struct EnvSettings
{
   QString  pubHost;
   int      pubPort;
};

bool operator == (const EnvSettings& l, const EnvSettings& r)
{
   return l.pubHost == r.pubHost
      && l.pubPort == r.pubPort;
}

NetworkSettingsPage::NetworkSettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::NetworkSettingsPage}
{
   ui_->setupUi(this);

   QMetaEnum metaEnumEnvConfiguration = QMetaEnum::fromType<ApplicationSettings::EnvConfiguration>();
   for (int i = 0; i < metaEnumEnvConfiguration.keyCount(); ++i) {
      ui_->comboBoxEnvironment->addItem(tr(metaEnumEnvConfiguration.valueToKey(i)));
   }
   ui_->comboBoxEnvironment->setCurrentIndex(-1);

   connect(ui_->pushButtonManageArmory, &QPushButton::clicked, this, [this](){
      // workaround here - wrap widget by QDialog
      // TODO: fix stylesheet to support popup widgets

      QDialog *d = new QDialog(this);
      QVBoxLayout *l = new QVBoxLayout(d);
      l->setContentsMargins(0,0,0,0);
      d->setLayout(l);
      d->setWindowTitle(tr("ArmoryDB connection"));
      d->resize(847, 593);

      ArmoryServersWidget *armoryServersWidget = new ArmoryServersWidget(armoryServersProvider_, appSettings_, this);

//      armoryServersWidget->setWindowModality(Qt::ApplicationModal);
//      armoryServersWidget->setWindowFlags(Qt::Dialog);
      l->addWidget(armoryServersWidget);

      connect(armoryServersWidget, &ArmoryServersWidget::reconnectArmory, this, [this](){
         emit reconnectArmory();
      });
      connect(armoryServersWidget, &ArmoryServersWidget::needClose, this, [d](){
         d->reject();
      });

      d->exec();
      emit armoryServerChanged();
   });

   connect(ui_->pushButtonArmoryServerKeyCopy, &QPushButton::clicked, this, [this](){
      qApp->clipboard()->setText(ui_->labelArmoryServerKey->text());
      ui_->pushButtonArmoryServerKeyCopy->setEnabled(false);
      ui_->pushButtonArmoryServerKeyCopy->setText(tr("Copied"));
      QTimer::singleShot(2000, [this](){
         ui_->pushButtonArmoryServerKeyCopy->setEnabled(true);
         ui_->pushButtonArmoryServerKeyCopy->setText(tr("Copy"));
      });
   });
   connect(ui_->pushButtonArmoryServerKeySave, &QPushButton::clicked, this, [this](){
      QString fileName = QFileDialog::getSaveFileName(this
                                   , tr("Save Armory Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + QStringLiteral("Armory_Server_Public_Key.pub")
                                   , tr("Key files (*.pub)"));

      QFile file(fileName);
      if (file.open(QIODevice::WriteOnly)) {
         file.write(ui_->labelArmoryServerKey->text().toLatin1());
      }
   });
}

void NetworkSettingsPage::initSettings()
{
   armoryServerModel_ = new ArmoryServersViewModel(armoryServersProvider_);
   armoryServerModel_->setSingleColumnMode(true);
   armoryServerModel_->setHighLightSelectedServer(false);
   ui_->comboBoxArmoryServer->setModel(armoryServerModel_);

   connect(armoryServersProvider_.get(), &ArmoryServersProvider::dataChanged, this, &NetworkSettingsPage::displayArmorySettings);
   connect(ui_->comboBoxEnvironment, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NetworkSettingsPage::onEnvSelected);
}

NetworkSettingsPage::~NetworkSettingsPage() = default;

void NetworkSettingsPage::display()
{
#ifdef PRODUCTION_BUILD
   ui_->PublicBridgeSettingsGroup->hide();
#endif

   displayArmorySettings();
   displayEnvironmentSettings();
}

void NetworkSettingsPage::displayArmorySettings()
{
   // set index of selected server
   ArmoryServer selectedServer = armoryServersProvider_->getArmorySettings();
   int selectedServerIndex = armoryServersProvider_->indexOfCurrent();
   ui_->comboBoxArmoryServer->setCurrentIndex(selectedServerIndex);


   // display info of connected server
   ArmorySettings connectedServerSettings = armoryServersProvider_->connectedArmorySettings();
   int connectedServerIndex = armoryServersProvider_->indexOfConnected();

   ui_->labelArmoryServerNetwork->setText(connectedServerSettings.netType == NetworkType::MainNet ? tr("MainNet") : tr("TestNet"));
   ui_->labelArmoryServerAddress->setText(connectedServerSettings.armoryDBIp);
   ui_->labelArmoryServerPort->setText(QString::number(connectedServerSettings.armoryDBPort));
   ui_->labelArmoryServerKey->setText(connectedServerSettings.armoryDBKey);

   // display tip if configuration was changed
   if (selectedServerIndex != connectedServerIndex
       || selectedServer != static_cast<ArmoryServer>(connectedServerSettings)) {
      ui_->labelConfChanged->setVisible(true);
   }
   else {
      ui_->labelConfChanged->setVisible(false);
   }
}

void NetworkSettingsPage::displayEnvironmentSettings()
{
   auto env = appSettings_->get<int>(ApplicationSettings::envConfiguration);
   ui_->comboBoxEnvironment->setCurrentIndex(env);
   ui_->lineEditCustomPubBridgeHost->setText(appSettings_->get<QString>(ApplicationSettings::customPubBridgeHost));
   ui_->spinBoxCustomPubBridgePort->setValue(appSettings_->get<int>(ApplicationSettings::customPubBridgePort));
   onEnvSelected(env);
}

void NetworkSettingsPage::reset()
{
   for (const auto &setting : {
        ApplicationSettings::runArmoryLocally,
        ApplicationSettings::netType,
        ApplicationSettings::envConfiguration,
        ApplicationSettings::customPubBridgeHost,
        ApplicationSettings::customPubBridgePort,
        ApplicationSettings::armoryDbIp,
        ApplicationSettings::armoryDbPort}) {
      appSettings_->reset(setting, false);
   }
   display();
}

void NetworkSettingsPage::apply()
{
   armoryServersProvider_->setupServer(ui_->comboBoxArmoryServer->currentIndex());

   appSettings_->set(ApplicationSettings::envConfiguration, ui_->comboBoxEnvironment->currentIndex());
   appSettings_->set(ApplicationSettings::customPubBridgeHost, ui_->lineEditCustomPubBridgeHost->text());
   appSettings_->set(ApplicationSettings::customPubBridgePort, ui_->spinBoxCustomPubBridgePort->value());
}

void NetworkSettingsPage::onEnvSelected(int index)
{
   auto env = ApplicationSettings::EnvConfiguration(index);
   const bool isCustom = (env == ApplicationSettings::Custom);
   ui_->lineEditCustomPubBridgeHost->setVisible(isCustom);
   ui_->spinBoxCustomPubBridgePort->setVisible(isCustom);
   ui_->labelCustomPubBridgeHost->setVisible(isCustom);
   ui_->labelCustomPubBridgePort->setVisible(isCustom);
}
