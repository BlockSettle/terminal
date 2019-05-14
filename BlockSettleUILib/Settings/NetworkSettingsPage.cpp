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

   connect(ui_->pushButtonArmoryTerminalKeyCopy, &QPushButton::clicked, this, [this](){
      qApp->clipboard()->setText(ui_->labelArmoryTerminalKey->text());
      ui_->pushButtonArmoryTerminalKeyCopy->setEnabled(false);
      ui_->pushButtonArmoryTerminalKeyCopy->setText(tr("Copied"));
      QTimer::singleShot(2000, [this](){
         ui_->pushButtonArmoryTerminalKeyCopy->setEnabled(true);
         ui_->pushButtonArmoryTerminalKeyCopy->setText(tr("Copy"));
      });
   });
   connect(ui_->pushButtonArmoryTerminalKeySave, &QPushButton::clicked, this, [this](){
      QString fileName = QFileDialog::getSaveFileName(this
                                   , tr("Save Armory Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + QStringLiteral("Terminal_Public_Key.pub")
                                   , tr("Key files (*.pub)"));

      QFile file(fileName);
      if (file.open(QIODevice::WriteOnly)) {
         file.write(ui_->labelArmoryTerminalKey->text().toLatin1());
      }
   });

   connect(armoryServersProvider_.get(), &ArmoryServersProvider::dataChanged, this, &NetworkSettingsPage::display);
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

void NetworkSettingsPage::DetectEnvironmentSettings()
{
   ApplicationSettings::EnvConfiguration conf = ApplicationSettings::EnvConfiguration::Custom;

   EnvSettings currentConfiguration{
      ui_->lineEditPublicBridgeHost->text(),
      ui_->spinBoxPublicBridgePort->value()
   };

   if (currentConfiguration == StagingEnvSettings) {
      conf = ApplicationSettings::EnvConfiguration::Staging;
   } else if (currentConfiguration == UATEnvSettings) {
      conf = ApplicationSettings::EnvConfiguration::UAT;
   } else if (currentConfiguration == PRODEnvSettings) {
      conf = ApplicationSettings::EnvConfiguration::PROD;
   }

   ui_->comboBoxEnvironment->setCurrentIndex(int(conf));
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

   try {
      RemoteSigner *remoteSigner = qobject_cast<RemoteSigner *>(signContainer_.get());
      if (remoteSigner) {
         SecureBinaryData ownKey = remoteSigner->getOwnPubKey();
         ui_->labelArmoryTerminalKey->setText(QString::fromStdString(ownKey.toHexStr()));
      }
   }
   catch (...) {
      ui_->labelArmoryTerminalKey->setText(tr("Unknown"));
   }
}

void NetworkSettingsPage::displayEnvironmentSettings()
{
   ui_->lineEditPublicBridgeHost->setText(appSettings_->get<QString>(ApplicationSettings::pubBridgeHost));
   ui_->spinBoxPublicBridgePort->setValue(appSettings_->get<int>(ApplicationSettings::pubBridgePort));
   ui_->spinBoxPublicBridgePort->setEnabled(false);

   DetectEnvironmentSettings();
}

void NetworkSettingsPage::reset()
{
   for (const auto &setting : {
        ApplicationSettings::runArmoryLocally,
        ApplicationSettings::netType,
        ApplicationSettings::pubBridgeHost,
        ApplicationSettings::pubBridgePort,
        ApplicationSettings::armoryDbIp,
        ApplicationSettings::armoryDbPort}) {
      appSettings_->reset(setting, false);
   }
   display();
}

void NetworkSettingsPage::apply()
{
   armoryServersProvider_->setupServer(ui_->comboBoxArmoryServer->currentIndex());

   appSettings_->set(ApplicationSettings::pubBridgeHost, ui_->lineEditPublicBridgeHost->text());
   appSettings_->set(ApplicationSettings::pubBridgePort, ui_->spinBoxPublicBridgePort->value());

   appSettings_->set(ApplicationSettings::envConfiguration, ui_->comboBoxEnvironment->currentIndex());
}

void NetworkSettingsPage::onEnvSelected(int index)
{
   ApplicationSettings::EnvConfiguration conf = ApplicationSettings::EnvConfiguration(index);

   if (conf == ApplicationSettings::EnvConfiguration::Custom) {
      ui_->lineEditPublicBridgeHost->setEnabled(true);
      ui_->spinBoxPublicBridgePort->setEnabled(true);
      return;
   }

   const EnvSettings *settings = nullptr;

   switch (conf) {
   case ApplicationSettings::EnvConfiguration::UAT:
      settings = &UATEnvSettings;
      break;
   case ApplicationSettings::EnvConfiguration::Staging:
      settings = &StagingEnvSettings;
      break;
   default:
      settings = &PRODEnvSettings;
      break;
   }

   ui_->lineEditPublicBridgeHost->setText(settings->pubHost);
   ui_->lineEditPublicBridgeHost->setEnabled(false);
   ui_->spinBoxPublicBridgePort->setValue(settings->pubPort);
   ui_->spinBoxPublicBridgePort->setEnabled(false);
}
