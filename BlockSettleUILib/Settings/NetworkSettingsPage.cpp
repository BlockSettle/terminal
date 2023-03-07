/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
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
#include "Wallets/HeadlessContainer.h"
#include "ArmoryServersViewModel.h"
#include "Settings/SignerSettings.h"
#include "SignersProvider.h"


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

      QDialog *dlg = new QDialog(this);
      QVBoxLayout *layout = new QVBoxLayout(dlg);
      layout->setContentsMargins(0,0,0,0);
      dlg->setLayout(layout);
      dlg->setWindowTitle(tr("BlockSettleDB connection"));
      dlg->resize(847, 593);  //FIXME: use custom dialog from resources

      armoryServersWidget_ =  appSettings_
         ? new ArmoryServersWidget(armoryServersProvider_, appSettings_, this)
         : new ArmoryServersWidget(this);

//      armoryServersWidget->setWindowModality(Qt::ApplicationModal);
//      armoryServersWidget->setWindowFlags(Qt::Dialog);
      layout->addWidget(armoryServersWidget_);

      connect(dlg, &QDialog::finished, [this] {
         armoryServersWidget_->deleteLater();
         armoryServersWidget_ = nullptr;
      });
      connect(armoryServersWidget_, &ArmoryServersWidget::reconnectArmory, this, [this](){
         emit reconnectArmory();
      });
      connect(armoryServersWidget_, &ArmoryServersWidget::addServer, this
         , &NetworkSettingsPage::addArmoryServer);
      connect(armoryServersWidget_, &ArmoryServersWidget::setServer, this
         , &NetworkSettingsPage::setArmoryServer);
      connect(armoryServersWidget_, &ArmoryServersWidget::delServer, this
         , &NetworkSettingsPage::delArmoryServer);
      connect(armoryServersWidget_, &ArmoryServersWidget::updServer, this
         , &NetworkSettingsPage::updArmoryServer);
      connect(armoryServersWidget_, &ArmoryServersWidget::needClose, this, [dlg](){
         dlg->reject();
      });

      armoryServersWidget_->onArmoryServers(armoryServers_, armorySrvCurrent_
         , armorySrvConnected_);
      dlg->exec();
      emit armoryServerChanged();
      // Switch env if needed
      onArmorySelected(ui_->comboBoxArmoryServer->currentIndex());
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
                                   , tr("Save BlockSettleDB Public Key")
                                   , QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + QStringLiteral("Armory_Server_Public_Key.pub")
                                   , tr("Key files (*.pub)"));

      QFile file(fileName);
      if (file.open(QIODevice::WriteOnly)) {
         file.write(ui_->labelArmoryServerKey->text().toLatin1());
      }
   });
}

void NetworkSettingsPage::init(const ApplicationSettings::State& state)
{
   if (state.find(ApplicationSettings::envConfiguration) == state.end()) {
      return;  // not our snapshot
   }
   SettingsPage::init(state);
}

void NetworkSettingsPage::onArmoryServers(const QList<ArmoryServer>& servers
   , int idxCur, int idxConn)
{
   armoryServers_ = servers;
   armorySrvCurrent_ = idxCur;
   armorySrvConnected_ = idxConn;
   if (armoryServerModel_) {
      armoryServerModel_->onArmoryServers(servers, idxCur, idxConn);
   }
   if (armoryServersWidget_) {
      armoryServersWidget_->onArmoryServers(servers, idxCur, idxConn);
   }
   displayArmorySettings();
}

void NetworkSettingsPage::initSettings()
{
   if (armoryServersProvider_) {
      armoryServerModel_ = new ArmoryServersViewModel(armoryServersProvider_);
      //connect(armoryServersProvider_.get(), &ArmoryServersProvider::dataChanged, this, &NetworkSettingsPage::displayArmorySettings);
   }
   else {
      armoryServerModel_ = new ArmoryServersViewModel(this);
   }
   armoryServerModel_->setSingleColumnMode(true);
   armoryServerModel_->setHighLightSelectedServer(false);
   ui_->comboBoxArmoryServer->setModel(armoryServerModel_);

   connect(ui_->comboBoxEnvironment, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NetworkSettingsPage::onEnvSelected);
   connect(ui_->comboBoxArmoryServer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NetworkSettingsPage::onArmorySelected);
}

NetworkSettingsPage::~NetworkSettingsPage() = default;

void NetworkSettingsPage::display()
{
   displayArmorySettings();
   displayEnvironmentSettings();

   disableSettingUpdate_ = false;
}

void NetworkSettingsPage::displayArmorySettings()
{
   ArmoryServer selectedServer;
   int selectedServerIndex = 0;
   ArmoryServer connectedServerSettings;
   int connectedServerIndex = 0;
   if (armoryServersProvider_) {
      // set index of selected server
      selectedServer = armoryServersProvider_->getArmorySettings();
      selectedServerIndex = armoryServersProvider_->indexOfCurrent();
      const auto& connectedIdx = armoryServersProvider_->indexOfCurrent();
      if (connectedIdx >= 0) {
         connectedServerSettings = armoryServersProvider_->servers().at(connectedIdx);
      }
      connectedServerIndex = armoryServersProvider_->indexOfConnected();
   }
   else {
      selectedServerIndex = armorySrvCurrent_;
      if ((selectedServerIndex >= armoryServers_.size()) || (selectedServerIndex < 0)) {
         return;
      }
      selectedServer = armoryServers_.at(selectedServerIndex);
      connectedServerIndex = armorySrvConnected_;
      if ((connectedServerIndex >= armoryServers_.size()) || (connectedServerIndex < 0)) {
         return;
      }
      connectedServerSettings = armoryServers_.at(connectedServerIndex);
   }
   ui_->comboBoxArmoryServer->setCurrentIndex(selectedServerIndex);

   // display info of connected server
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
   int env = 0;
   if (appSettings_) {
      env = appSettings_->get<int>(ApplicationSettings::envConfiguration);
   }
   else {
      env = settings_.at(ApplicationSettings::envConfiguration).toInt();
   }
   ui_->comboBoxEnvironment->setCurrentIndex(env);
   onEnvSelected(env);
}

void NetworkSettingsPage::applyLocalSignerNetOption()
{
   NetworkType networkType = static_cast<NetworkType>(appSettings_->get(ApplicationSettings::netType).toInt());
   SignerSettings settings;
   settings.setTestNet(networkType == NetworkType::TestNet);
}

void NetworkSettingsPage::reset()
{
   const std::vector<ApplicationSettings::Setting> resetList{
      ApplicationSettings::runArmoryLocally, ApplicationSettings::netType
      , ApplicationSettings::envConfiguration, ApplicationSettings::armoryDbIp
      , ApplicationSettings::armoryDbPort
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

void NetworkSettingsPage::apply()
{
   if (armoryServersProvider_ && appSettings_ && signersProvider_) {
      armoryServersProvider_->setupServer(ui_->comboBoxArmoryServer->currentIndex());

      appSettings_->set(ApplicationSettings::envConfiguration, ui_->comboBoxEnvironment->currentIndex());

      if (signersProvider_->currentSignerIsLocal()) {
         applyLocalSignerNetOption();
      }
   }
   else {
      emit setArmoryServer(ui_->comboBoxArmoryServer->currentIndex());
      emit putSetting(ApplicationSettings::envConfiguration, ui_->comboBoxEnvironment->currentIndex());
   }
}

void NetworkSettingsPage::onEnvSelected(int envIndex)
{
   if (disableSettingUpdate_) {
      return;
   }
   const auto env = ApplicationSettings::EnvConfiguration(envIndex);
   const int armoryIndex = ui_->comboBoxArmoryServer->currentIndex();
   int serverIndex = armoryIndex;
   QList<ArmoryServer> armoryServers;
   if (armoryServersProvider_) {
      for (const auto& server : armoryServersProvider_->servers()) {
         armoryServers.append(server);
      }
   }
   else {
      armoryServers = armoryServers_;
   }
   if (armoryIndex < 0 || armoryIndex >= armoryServers.count()) {
      return;
   }
   auto armoryServer = armoryServers[armoryIndex];
   if ((armoryServer.netType == NetworkType::MainNet) != (env == ApplicationSettings::EnvConfiguration::Production)) {
      if (env == ApplicationSettings::EnvConfiguration::Production) {
         serverIndex = ArmoryServersProvider::getIndexOfMainNetServer();
      } else {
         serverIndex = ArmoryServersProvider::getIndexOfTestNetServer();
      }
   }
   ui_->comboBoxArmoryServer->setCurrentIndex(serverIndex);
}

void NetworkSettingsPage::onArmorySelected(int armoryIndex)
{
   int envIndex = ui_->comboBoxEnvironment->currentIndex();
   QList<ArmoryServer> armoryServers;
   if (armoryServersProvider_) {
      for (const auto& server : armoryServersProvider_->servers()) {
         armoryServers.append(server);
      }
   }
   else {
      armoryServers = armoryServers_;
   }
   if (armoryIndex < 0 || armoryIndex >= armoryServers.count()) {
      return;
   }
   auto armoryServer = armoryServers[armoryIndex];
   const auto envSelected = static_cast<ApplicationSettings::EnvConfiguration>(ui_->comboBoxEnvironment->currentIndex());

   if ((armoryServer.netType == NetworkType::MainNet) != (envSelected == ApplicationSettings::EnvConfiguration::Production)) {
      if (armoryServer.netType == NetworkType::MainNet) {
         envIndex = static_cast<int>(ApplicationSettings::EnvConfiguration::Production);
      } else {
         envIndex = static_cast<int>(ApplicationSettings::EnvConfiguration::Test);
      }
   }
   ui_->comboBoxEnvironment->setCurrentIndex(envIndex);
}
