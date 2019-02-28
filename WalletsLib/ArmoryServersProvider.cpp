#include "ArmoryServersProvider.h"

#include <QDir>
#include <QStandardPaths>

ArmoryServersProvider::ArmoryServersProvider(const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent)
   : appSettings_(appSettings)
   , QObject(parent)
{

}

QList<ArmoryServer> ArmoryServersProvider::servers() const
{
   QStringList userServers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);

   QList<ArmoryServer> servers;

   QStringList defaultServersKeys = appSettings_->get<QStringList>(ApplicationSettings::defaultArmoryServersKeys);

   // #1 add MainNet blocksettle server
   ArmoryServer bsMainNet = ArmoryServer::fromTextSettings(QStringLiteral("BlockSettle MainNet:0:armory.blocksettle.com:80:"));
   if (defaultServersKeys.size() >= 1) {
      bsMainNet.armoryDBKey = defaultServersKeys.at(0);
   }
   servers.append(bsMainNet);

   // #2 add TestNet blocksettle server
   ArmoryServer bsTestNet = ArmoryServer::fromTextSettings(QStringLiteral("BlockSettle TestNet:1:armory.blocksettle.com:81:"));
   if (defaultServersKeys.size() >= 2) {
      bsTestNet.armoryDBKey = defaultServersKeys.at(1);
   }
   servers.append(bsTestNet);

   // #3 add localhost node MainNet
   ArmoryServer localMainNet;
   localMainNet.netType = NetworkType::MainNet;
   localMainNet.armoryDBIp = QStringLiteral("127.0.0.1");
   localMainNet.armoryDBPort = appSettings_->GetDefaultArmoryRemotePort(NetworkType::MainNet);
   localMainNet.name =  QStringLiteral("Local Node MainNet");
   if (defaultServersKeys.size() >= 3) {
      localMainNet.armoryDBKey = defaultServersKeys.at(2);
   }
   servers.append(localMainNet);

   // #4 add localhost node TestNet
   ArmoryServer localTestNet;
   localTestNet.netType = NetworkType::TestNet;
   localTestNet.armoryDBIp = QStringLiteral("127.0.0.1");
   localTestNet.armoryDBPort = appSettings_->GetDefaultArmoryRemotePort(NetworkType::TestNet);
   localTestNet.name =  QStringLiteral("Local Node TestNet");
   if (defaultServersKeys.size() >= 4) {
      localTestNet.armoryDBKey = defaultServersKeys.at(3);
   }
   servers.append(localTestNet);

   for (const QString &srv : userServers) {
      servers.append(ArmoryServer::fromTextSettings(srv));
   }

   return servers;
}

ArmorySettings ArmoryServersProvider::getArmorySettings() const
{
   ArmorySettings settings;

   settings.netType = appSettings_->get<NetworkType>(ApplicationSettings::netType);
   settings.runLocally = appSettings_->get<bool>(ApplicationSettings::runArmoryLocally);
   if (settings.runLocally) {
      settings.armoryDBIp = QStringLiteral("127.0.0.1");
      settings.armoryDBPort = appSettings_->GetDefaultArmoryLocalPort(appSettings_->get<NetworkType>(ApplicationSettings::netType));
   } else {
      settings.armoryDBIp = appSettings_->get<QString>(ApplicationSettings::armoryDbIp);
      settings.armoryDBPort = appSettings_->GetArmoryRemotePort();
   }
   settings.socketType = appSettings_->GetArmorySocketType();

   settings.armoryExecutablePath = QDir::cleanPath(appSettings_->get<QString>(ApplicationSettings::armoryPathName));
   settings.dbDir = appSettings_->GetDBDir();
   settings.bitcoinBlocksDir = appSettings_->GetBitcoinBlocksDir();
   settings.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

   return settings;
}

int ArmoryServersProvider::indexOf(const QString &name) const
{
   // naive implementation
   QList<ArmoryServer> s = servers();
   for (int i = 0; i < s.size(); ++i) {
      if (s.at(i).name == name) {
         return i;
      }
   }
   return -1;
}

int ArmoryServersProvider::indexOfIpPort(const std::string &srvIPPort) const
{
   QString ipPort = QString::fromStdString(srvIPPort);
   QStringList ipPortList = ipPort.split(QStringLiteral(":"));
   if (ipPortList.size() != 2) {
      return -1;
   }

   for (int i = 0; i < servers().size(); ++i) {
      if (servers().at(i).armoryDBIp == ipPortList.at(0) && servers().at(i).armoryDBPort == ipPortList.at(1).toInt()) {
         return i;
      }
   }
   return -1;
}

void ArmoryServersProvider::add(const ArmoryServer &server)
{
   QStringList servers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   servers.append(server.toTextSettings());
   appSettings_->set(ApplicationSettings::armoryServers, servers);
   emit dataChanged();
}

void ArmoryServersProvider::remove(int index)
{
   if (index < defaultServersCount) {
      return;
   }

   QStringList servers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   int indexToRemove = index - defaultServersCount;
   if (indexToRemove > 0 && indexToRemove < servers.size()){
      servers.removeAt(indexToRemove);
      appSettings_->set(ApplicationSettings::armoryServers, servers);
      emit dataChanged();
   }
}

void ArmoryServersProvider::setupServer(int index)
{
   QList<ArmoryServer> srv = servers();
   if (index > 0 && index <= srv.size()) {
      ArmoryServer server = srv.at(index);
      appSettings_->set(ApplicationSettings::armoryDbName, server.name);
      appSettings_->set(ApplicationSettings::armoryDbIp, server.armoryDBIp);
      appSettings_->set(ApplicationSettings::armoryDbPort, server.armoryDBPort);
      appSettings_->set(ApplicationSettings::netType, static_cast<int>(server.netType));
      appSettings_->set(ApplicationSettings::runArmoryLocally, server.runLocally);
   }
}

void ArmoryServersProvider::addKey(const QString &address, int port, const QString &key)
{
   // find server
   int index = -1;
   for (int i = 0; i < servers().size(); ++i) {
      if (servers().at(i).armoryDBIp == address && servers().at(i).armoryDBPort == port) {
         index = i;
         break;
      }
   }

   if (index == -1){
      return;
   }

   if (index < ArmoryServersProvider::defaultServersCount) {
      QStringList defaultKeys = appSettings_->get<QStringList>(ApplicationSettings::defaultArmoryServersKeys);

      // defaultKeys might be empty
      while (defaultKeys.size() <= index) {
         defaultKeys.append(QString());
      }

      defaultKeys[index] = key;
      appSettings_->set(ApplicationSettings::defaultArmoryServersKeys, defaultKeys);
   }
   else {
      QStringList servers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
      QString serverTxt = servers.at(index);
      ArmoryServer server = ArmoryServer::fromTextSettings(serverTxt);
      server.armoryDBKey = key;
      servers[index] = server.toTextSettings();

      appSettings_->set(ApplicationSettings::armoryServers, servers);
   }

   emit dataChanged();
}

void ArmoryServersProvider::addKey(const std::string &srvIPPort, const BinaryData &srvPubKey)
{
   QString ipPort = QString::fromStdString(srvIPPort);
   QStringList ipPortList = ipPort.split(QStringLiteral(":"));
   if (ipPortList.size() == 2) {
      addKey(ipPortList.at(0)
             , ipPortList.at(1).toInt()
             , QString::fromLatin1(QByteArray::fromStdString(srvPubKey.toBinStr()).toHex()));
   }
}


