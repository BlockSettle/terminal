#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>

#include "NetworkSettingsPage.h"
#include "ui_NetworkSettingsPage.h"
#include "ApplicationSettings.h"
#include "ArmoryServersWidget.h"

enum EnvConfiguration
{
   CustomConfiguration,
   StagingConfiguration,
   UATConfiguration,
   PRODConfiguration
};

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

static const EnvSettings StagingEnvSettings {
   QLatin1String("185.213.153.45"), 9091 };

static const EnvSettings UATEnvSettings{
   QLatin1String("185.213.153.44"), 9091 };

static const EnvSettings PRODEnvSettings{
   QLatin1String("185.213.153.36"), 9091 };

NetworkSettingsPage::NetworkSettingsPage(QWidget* parent)
   : SettingsPage{parent}
   , ui_{new Ui::NetworkSettingsPage}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonManageArmory, &QPushButton::clicked, this, [this](){
      ArmoryServersWidget armoryServersWidget(armoryServersProvider_, this);
      connect(&armoryServersWidget, &ArmoryServersWidget::reconnectArmory, this, [this](){
         emit reconnectArmory();
      });
      armoryServersWidget.exec();
   });

   connect(ui_->pushButtonArmoryServerKeyCopy, &QPushButton::clicked, this, [this](){
      qApp->clipboard()->setText(ui_->labelArmoryServerKey->text());
   });
   connect(ui_->pushButtonArmoryServerKeySave, &QPushButton::clicked, this, [this](){
      QString fileName = QFileDialog::getSaveFileName(this
                                   , tr("Save Armory Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                   , tr("Key files (*.pub)"));

      QFile file(fileName);
      if (file.open(QIODevice::WriteOnly)) {
         file.write(ui_->labelArmoryServerKey->text().toLatin1());
      }
   });

   connect(ui_->pushButtonArmoryTerminalKeyCopy, &QPushButton::clicked, this, [this](){
      qApp->clipboard()->setText(ui_->labelArmoryTerminalKey->text());
   });
   connect(ui_->pushButtonArmoryTerminalKeySave, &QPushButton::clicked, this, [this](){
      QString fileName = QFileDialog::getSaveFileName(this
                                   , tr("Save Armory Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                   , tr("Key files (*.pub)"));

      QFile file(fileName);
      if (file.open(QIODevice::WriteOnly)) {
         file.write(ui_->labelArmoryTerminalKey->text().toLatin1());
      }
   });


   ui_->PublicBridgeSettingsGroup->hide();
}

void NetworkSettingsPage::initSettings()
{
   armoryServerModel_ = new ArmoryServersViewModel(armoryServersProvider_);
   ui_->comboBoxArmoryServer->setModel(armoryServerModel_);

   connect(armoryServersProvider_.get(), &ArmoryServersProvider::dataChanged, this, [this](){
      display();
   });

   connect(ui_->comboBoxEnvironment, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NetworkSettingsPage::onEnvSelected);
}

NetworkSettingsPage::~NetworkSettingsPage() = default;

void NetworkSettingsPage::display()
{
#ifdef PRODUCTION_BUILD
   ui_->PublicBridgeSettingsGroup->hide();
#endif
   ui_->lineEditPublicBridgeHost->setText(appSettings_->get<QString>(ApplicationSettings::pubBridgeHost));
   ui_->lineEditPublicBridgeHost->setEnabled(false);
   ui_->spinBoxPublicBridgePort->setValue(appSettings_->get<int>(ApplicationSettings::pubBridgePort));

   ui_->lineEditPublicBridgeHost->setText(appSettings_->get<QString>(ApplicationSettings::pubBridgeHost));
   ui_->spinBoxPublicBridgePort->setValue(appSettings_->get<int>(ApplicationSettings::pubBridgePort));
   ui_->spinBoxPublicBridgePort->setEnabled(false);

   int serverIndex = armoryServersProvider_->indexOf(appSettings_->get<QString>(ApplicationSettings::armoryDbName));
   ArmoryServer server = armoryServersProvider_->servers().at(serverIndex);

   int port = appSettings_->GetArmoryRemotePort(server.netType);
   ui_->comboBoxArmoryServer->setCurrentIndex(serverIndex);

   ui_->labelArmoryServerNetwork->setText(server.netType == NetworkType::MainNet ? tr("MainNet") : tr("TestNet"));
   ui_->labelArmoryServerAddress->setText(server.armoryDBIp);
   ui_->labelArmoryServerPort->setText(QString::number(port));
   ui_->labelArmoryServerKey->setText(server.armoryDBKey);

   ui_->comboBoxEnvironment->addItem(tr("Custom"));
   ui_->comboBoxEnvironment->addItem(tr("Staging"));
   ui_->comboBoxEnvironment->addItem(tr("UAT"));
   ui_->comboBoxEnvironment->addItem(tr("PROD"));

   ui_->comboBoxEnvironment->setCurrentIndex(-1);

   DetectEnvironmentSettings();
}

void NetworkSettingsPage::DetectEnvironmentSettings()
{
   int index = CustomConfiguration;

   EnvSettings currentConfiguration{
      ui_->lineEditPublicBridgeHost->text(),
      ui_->spinBoxPublicBridgePort->value()
   };

   if (currentConfiguration == StagingEnvSettings) {
      index = StagingConfiguration;
   } else if (currentConfiguration == UATEnvSettings) {
      index = UATConfiguration;
   } else if (currentConfiguration == PRODEnvSettings) {
      index = PRODConfiguration;
   }

   ui_->comboBoxEnvironment->setCurrentIndex(index);
}

void NetworkSettingsPage::reset()
{
   for (const auto &setting : { ApplicationSettings::runArmoryLocally, ApplicationSettings::netType
      , ApplicationSettings::pubBridgeHost, ApplicationSettings::pubBridgePort
      , ApplicationSettings::armoryDbIp, ApplicationSettings::armoryDbPort}) {
      appSettings_->reset(setting, false);
   }
   display();
}

void NetworkSettingsPage::apply()
{
   applyArmoryServers();

   appSettings_->set(ApplicationSettings::pubBridgeHost, ui_->lineEditPublicBridgeHost->text());
   appSettings_->set(ApplicationSettings::pubBridgePort, ui_->spinBoxPublicBridgePort->value());
}

void NetworkSettingsPage::applyArmoryServers()
{
   armoryServersProvider_->setupServer(ui_->comboBoxArmoryServer->currentIndex());
}

void NetworkSettingsPage::onEnvSelected(int index)
{
   if (index != CustomConfiguration) {
      const EnvSettings *settings = nullptr;
      if (index == StagingConfiguration) {
         settings = &StagingEnvSettings;
      } else if (index == UATConfiguration) {
         settings = &UATEnvSettings;
      } else  {
         settings = &PRODEnvSettings;
      }

      ui_->lineEditPublicBridgeHost->setText(settings->pubHost);
      ui_->lineEditPublicBridgeHost->setEnabled(false);
      ui_->spinBoxPublicBridgePort->setValue(settings->pubPort);
      ui_->spinBoxPublicBridgePort->setEnabled(false);
   } else {
      ui_->lineEditPublicBridgeHost->setEnabled(true);
      ui_->spinBoxPublicBridgePort->setEnabled(true);
   }
}
