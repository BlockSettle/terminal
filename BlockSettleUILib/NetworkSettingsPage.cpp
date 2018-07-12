#include "NetworkSettingsPage.h"

#include "ui_NetworkSettingsPage.h"

#include "ApplicationSettings.h"

enum EnvConfiguration
{
   CustomConfiguration,
   StagingConfiguration,
   UATConfiguration
};

struct EnvSettings
{
   QString  celerHost;
   int      celerPort;

   QString  pubHost;
   int      pubPort;
};

bool operator == (const EnvSettings& l, const EnvSettings& r)
{
   return l.celerHost == r.celerHost
         && l.celerPort == r.celerPort
         && l.pubHost == r.pubHost
         && l.pubPort == r.pubPort;
}

static const EnvSettings StagingEnvSettings{
   QLatin1String("104.155.117.179"),
   16001,
   QLatin1String("193.138.218.44"),
   19091};

static const EnvSettings UATEnvSettings{
   QLatin1String("193.138.218.39"),
   16001,
   QLatin1String("193.138.218.36"),
   9091};

NetworkSettingsPage::NetworkSettingsPage(QWidget* parent)
   : QWidget{parent}
   , ui_{new Ui::NetworkSettingsPage}
{
   ui_->setupUi(this);

   ui_->comboBoxEnv->addItem(tr("Custom"));
   ui_->comboBoxEnv->addItem(tr("Staging"));
   ui_->comboBoxEnv->addItem(tr("UAT"));

   ui_->comboBoxEnv->setCurrentIndex(-1);
   ui_->comboBoxEnv->setEnabled(false);

   connect(ui_->runArmoryDBLocallyCheckBox, &QCheckBox::clicked
      , this, &NetworkSettingsPage::onRunArmoryLocallyChecked);

   connect(ui_->checkBoxTestnet, &QCheckBox::clicked
      , this, &NetworkSettingsPage::onNetworkClicked);

   connect(ui_->celerHostLineEdit, &QLineEdit::textEdited, this, &NetworkSettingsPage::onEnvSettingsChanged);
   connect(ui_->celerPortSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onEnvSettingsChanged()));

   connect(ui_->lineEditPublicBridgeHost, &QLineEdit::textEdited, this, &NetworkSettingsPage::onEnvSettingsChanged);
   connect(ui_->spinBoxPublicBridgePort, SIGNAL(valueChanged(int)), this, SLOT(onEnvSettingsChanged()));

   connect(ui_->comboBoxEnv, SIGNAL(currentIndexChanged(int)), this, SLOT(onEnvSelected(int)));
}

void NetworkSettingsPage::setAppSettings(const std::shared_ptr<ApplicationSettings>& appSettings)
{
   appSettings_ = appSettings;
}

void NetworkSettingsPage::displaySettings(bool displayDefault)
{
   ui_->checkBoxTestnet->setChecked(appSettings_->get<NetworkType>(ApplicationSettings::netType, displayDefault) == NetworkType::TestNet);

   if (appSettings_->get<bool>(ApplicationSettings::runArmoryLocally, displayDefault)) {
      ui_->runArmoryDBLocallyCheckBox->setChecked(true);
      DisplayRunArmorySettings(true, displayDefault);
   } else {
      ui_->runArmoryDBLocallyCheckBox->setChecked(false);
      DisplayRunArmorySettings(false, displayDefault);
   }

   ui_->celerHostLineEdit->setText(appSettings_->get<QString>(ApplicationSettings::celerHost, displayDefault));
   ui_->celerPortSpinBox->setValue(appSettings_->get<int>(ApplicationSettings::celerPort, displayDefault));

   ui_->lineEditPublicBridgeHost->setText(appSettings_->get<QString>(ApplicationSettings::pubBridgeHost, displayDefault));
   ui_->spinBoxPublicBridgePort->setValue(appSettings_->get<int>(ApplicationSettings::pubBridgePort, displayDefault));

   ui_->comboBoxEnv->setEnabled(true);
   DetectEnvironmentSettings();
}

void NetworkSettingsPage::DisplayRunArmorySettings(bool runLocally, bool displayDefault)
{
   if (runLocally) {
      ui_->armoryDBHostLineEdit->setText(QString::fromStdString("localhost"));

      auto networkType = ui_->checkBoxTestnet->isChecked()
         ? NetworkType::TestNet
         : NetworkType::MainNet;

      ui_->armoryDBPortSpinBox->setValue(appSettings_->GetDefaultArmoryPortForNetwork(networkType));
      ui_->armoryDBHostLineEdit->setEnabled(false);
      ui_->armoryDBPortSpinBox->setEnabled(false);
   } else {
      ui_->armoryDBHostLineEdit->setEnabled(true);
      ui_->armoryDBPortSpinBox->setEnabled(true);
      ui_->armoryDBHostLineEdit->setText(appSettings_->get<QString>(ApplicationSettings::armoryDbIp, displayDefault));
      ui_->armoryDBPortSpinBox->setValue(appSettings_->get<int>(ApplicationSettings::armoryDbPort, displayDefault));
   }
}

void NetworkSettingsPage::DetectEnvironmentSettings()
{
   int index = CustomConfiguration;

   EnvSettings currentConfiguration{
      ui_->celerHostLineEdit->text(),
      ui_->celerPortSpinBox->value(),
      ui_->lineEditPublicBridgeHost->text(),
      ui_->spinBoxPublicBridgePort->value()
   };

   if (currentConfiguration == StagingEnvSettings) {
      index = StagingConfiguration;
   } else if (currentConfiguration == UATEnvSettings) {
      index = UATConfiguration;
   }

   ui_->comboBoxEnv->setCurrentIndex(index);
}

void NetworkSettingsPage::applyChanges()
{
   appSettings_->set(ApplicationSettings::netType, ui_->checkBoxTestnet->isChecked()
      ? (int)NetworkType::TestNet : (int)NetworkType::MainNet);

   if (ui_->runArmoryDBLocallyCheckBox->isChecked()) {
      appSettings_->set(ApplicationSettings::runArmoryLocally, true);
   } else {
      appSettings_->set(ApplicationSettings::runArmoryLocally, false);
      appSettings_->set(ApplicationSettings::armoryDbIp, ui_->armoryDBHostLineEdit->text());
      appSettings_->set(ApplicationSettings::armoryDbPort, ui_->armoryDBPortSpinBox->value());
   }
   appSettings_->set(ApplicationSettings::celerHost, ui_->celerHostLineEdit->text());
   appSettings_->set(ApplicationSettings::celerPort, ui_->celerPortSpinBox->value());

   appSettings_->set(ApplicationSettings::pubBridgeHost, ui_->lineEditPublicBridgeHost->text());
   appSettings_->set(ApplicationSettings::pubBridgePort, ui_->spinBoxPublicBridgePort->value());
}

void NetworkSettingsPage::onRunArmoryLocallyChecked(bool checked)
{
   DisplayRunArmorySettings(checked, false);
}

void NetworkSettingsPage::onNetworkClicked(bool checked)
{
   if (ui_->runArmoryDBLocallyCheckBox->isChecked()) {
      DisplayRunArmorySettings(true, false);
   }
}

void NetworkSettingsPage::onEnvSettingsChanged()
{
   DetectEnvironmentSettings();
}

void NetworkSettingsPage::onEnvSelected(int index)
{
   if (index != CustomConfiguration) {
      const EnvSettings *settings = nullptr;
      if (index == StagingConfiguration) {
         settings = &StagingEnvSettings;
      } else {
         settings = &UATEnvSettings;
      }

      ui_->celerHostLineEdit->setText(settings->celerHost);
      ui_->celerPortSpinBox->setValue(settings->celerPort);
      ui_->lineEditPublicBridgeHost->setText(settings->pubHost);
      ui_->spinBoxPublicBridgePort->setValue(settings->pubPort);
   }
}
