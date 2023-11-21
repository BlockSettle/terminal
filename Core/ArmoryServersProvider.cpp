/*

***********************************************************************************
* Copyright (C) 2019 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ArmoryServersProvider.h"
#include "BootstrapDataManager.h"
#include <QDir>
#include <QStandardPaths>
#include <spdlog/spdlog.h>

namespace {

   // Change to true if local ArmoryDB auto-start should be reverted (not tested, see BST-2131)
   const bool kEnableLocalAutostart = false;

   enum class ServerIndices {
      MainNet = 0,
      TestNet,
      //LocalhostMainNet,
      //LocalHostTestNet
   };

} // namespace

const std::vector<ArmoryServer> ArmoryServersProvider::defaultServers_ = {
   ArmoryServer::fromTextSettings(ARMORY_BLOCKSETTLE_NAME":0:mainnet.blocksettle.com:9003:"),
   ArmoryServer::fromTextSettings(ARMORY_BLOCKSETTLE_NAME":1:testnet.blocksettle.com:19003:"),
/*   ArmoryServer::fromTextSettings(kEnableLocalAutostart ?
      QStringLiteral("%1:0:127.0.0.1::").arg(QObject::tr("Local Auto-launch Node")) :
      QStringLiteral("%1:0:127.0.0.1::").arg(QObject::tr("Local BlockSettleDB Node"))),
   ArmoryServer::fromTextSettings(kEnableLocalAutostart ?
      QStringLiteral("%1:1:127.0.0.1::").arg(QObject::tr("Local Auto-launch Node")) :
      QStringLiteral("%1:1:127.0.0.1:81:").arg(QObject::tr("Local BlockSettleDB Node")))*/
};

const int ArmoryServersProvider::kDefaultServersCount = ArmoryServersProvider::defaultServers_.size();

ArmoryServersProvider::ArmoryServersProvider(const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<spdlog::logger>& logger)
   : appSettings_(appSettings), logger_(logger)
{}

std::vector<ArmoryServer> ArmoryServersProvider::servers() const
{
   QStringList userServers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   std::vector<ArmoryServer> servers;

   // #1 add MainNet blocksettle server
   ArmoryServer bsMainNet = defaultServers_.at(static_cast<int>(ServerIndices::MainNet));
   //bsMainNet.armoryDBKey = QString::fromStdString(bootstrapDataManager_->getArmoryMainnetKey()); //hard-code key
   servers.push_back(bsMainNet);

   // #2 add TestNet blocksettle server
   ArmoryServer bsTestNet = defaultServers_.at(static_cast<int>(ServerIndices::TestNet));
   //bsTestNet.armoryDBKey = QString::fromStdString(bootstrapDataManager_->getArmoryTestnetKey());
   servers.push_back(bsTestNet);

/*   // #3 add localhost node MainNet
   ArmoryServer localMainNet = defaultServers_.at(static_cast<int>(ServerIndices::LocalhostMainNet));
   localMainNet.armoryDBPort = appSettings_->GetDefaultArmoryLocalPort(NetworkType::MainNet);
   localMainNet.runLocally = kEnableLocalAutostart;
   servers.push_back(localMainNet);

   // #4 add localhost node TestNet
   ArmoryServer localTestNet = defaultServers_.at(static_cast<int>(ServerIndices::LocalHostTestNet));
   localTestNet.armoryDBPort = appSettings_->GetDefaultArmoryLocalPort(NetworkType::TestNet);
   localTestNet.runLocally = kEnableLocalAutostart;
   servers.push_back(localTestNet);*/

   for (const QString &srv : userServers) {
      servers.push_back(ArmoryServer::fromTextSettings(srv.toStdString()));
   }
   return servers;
}

ArmorySettings ArmoryServersProvider::getArmorySettings() const
{
   ArmorySettings settings;

   settings.netType = appSettings_->get<NetworkType>(ApplicationSettings::netType);
   settings.armoryDBIp = appSettings_->get<std::string>(ApplicationSettings::armoryDbIp);
   settings.armoryDBPort = std::to_string(appSettings_->GetArmoryRemotePort());
   settings.runLocally = appSettings_->get<bool>(ApplicationSettings::runArmoryLocally);

   const int serverIndex = indexOf(static_cast<ArmoryServer>(settings));
   if (serverIndex >= 0) {
      settings.armoryDBKey = servers().at(serverIndex).armoryDBKey;
   }

   settings.socketType = appSettings_->GetArmorySocketType();

   settings.armoryExecutablePath = QDir::cleanPath(appSettings_->get<QString>(ApplicationSettings::armoryPathName)).toStdString();
   settings.dbDir = appSettings_->GetDBDir().toStdString();
   settings.bitcoinBlocksDir = appSettings_->GetBitcoinBlocksDir().toStdString();
   settings.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();

   return settings;
}

int ArmoryServersProvider::indexOfCurrent() const
{
   return indexOf(static_cast<ArmoryServer>(getArmorySettings()));
}

int ArmoryServersProvider::indexOfConnected() const
{
   return lastConnectedIndex_;
}

int ArmoryServersProvider::indexOf(const QString &name) const
{
   // naive implementation
   const auto& s = servers();
   for (int i = 0; i < s.size(); ++i) {
      if (s.at(i).name == name.toStdString()) {
         return i;
      }
   }
   return -1;
}

int ArmoryServersProvider::indexOf(const ArmoryServer &server) const
{
   const auto& srvrs = servers();
   for (int i = 0; i < srvrs.size(); ++i) {
      const auto& s = srvrs.at(i);
/*      logger_->debug("[{}] {}: {} vs {}, {} vs {}, {} vs {}, {} vs {}", __func__
         , i, s.name.toStdString(), server.name.toStdString(), (int)s.netType
         , (int)server.netType, s.armoryDBIp.toStdString()
         , server.armoryDBIp.toStdString(), s.armoryDBPort, server.armoryDBPort);*/
      if ((server.name.empty() || (s.name == server.name)) && (s.netType == server.netType)
         && (s.armoryDBIp == server.armoryDBIp) && (s.armoryDBPort == server.armoryDBPort)) {
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
      if ((servers().at(i).armoryDBIp == ipPortList.at(0).toStdString())
         && (servers().at(i).armoryDBPort == ipPortList.at(1).toStdString())) {
         return i;
      }
   }
   return -1;
}

int ArmoryServersProvider::getIndexOfMainNetServer()
{
   return static_cast<int>(ServerIndices::MainNet);
}

int ArmoryServersProvider::getIndexOfTestNetServer()
{
   return static_cast<int>(ServerIndices::TestNet);
}

bool ArmoryServersProvider::add(const ArmoryServer &server)
{
   if (server.armoryDBPort.empty()) {
      return false;
   }
   const int armoryPort = std::stoi(server.armoryDBPort);
   if (armoryPort < 1 || armoryPort > USHRT_MAX) {
      return false;
   }
   if (server.name.empty()) {
      return false;
   }

   const auto& serversData = servers();
   // check if server with already exist
   for (const ArmoryServer &s : serversData) {
      if (s.name == server.name) {
         return false;
      }
      if (s.armoryDBIp == server.armoryDBIp
          && s.armoryDBPort == server.armoryDBPort
          && s.netType == server.netType) {
         return false;
      }
   }

   QStringList serversTxt = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);

   serversTxt.append(QString::fromStdString(server.toTextSettings()));
   appSettings_->set(ApplicationSettings::armoryServers, serversTxt);
   return true;
}

bool ArmoryServersProvider::replace(int index, const ArmoryServer &server)
{
   if (server.armoryDBPort.empty() || server.name.empty()) {
      return false;
   }
   if (index < kDefaultServersCount) {
      return false;
   }

   const auto& serversData = servers();
   if (index >= serversData.size()) {
      return false;
   }

   // check if server with already exist
   for (int i = 0; i < serversData.size(); ++i) {
      if (i == index) continue;

      const ArmoryServer &s = serversData.at(i);
      if (s.name == server.name) {
         return false;
      }
      if (s.armoryDBIp == server.armoryDBIp
          && s.armoryDBPort == server.armoryDBPort
          && s.netType == server.netType) {
         return false;
      }
   }

   QStringList serversTxt = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   if (index - kDefaultServersCount >= serversTxt.size()) {
      return false;
   }

   serversTxt.replace(index - kDefaultServersCount, QString::fromStdString(server.toTextSettings()));
   appSettings_->set(ApplicationSettings::armoryServers, serversTxt);
   return true;
}

bool ArmoryServersProvider::remove(int index)
{
   if (index < kDefaultServersCount) {
      return false;
   }

   QStringList servers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   int indexToRemove = index - kDefaultServersCount;
   if (indexToRemove >= 0 && indexToRemove < servers.size()){
      servers.removeAt(indexToRemove);
      appSettings_->set(ApplicationSettings::armoryServers, servers);
      return true;
   }
   else {
      return false;
   }
}

NetworkType ArmoryServersProvider::setupServer(int index)
{
   NetworkType netType{ NetworkType::Invalid };
   const auto& srvList = servers();
   if (index >= 0 && index < srvList.size()) {
      const auto& server = srvList.at(index);
      appSettings_->set(ApplicationSettings::armoryDbName, QString::fromStdString(server.name));
      appSettings_->set(ApplicationSettings::armoryDbIp, QString::fromStdString(server.armoryDBIp));
      appSettings_->set(ApplicationSettings::armoryDbPort, QString::fromStdString(server.armoryDBPort));
      appSettings_->set(ApplicationSettings::netType, static_cast<int>(server.netType));
      appSettings_->set(ApplicationSettings::runArmoryLocally, server.runLocally);
      netType = server.netType;
   }
   lastConnectedIndex_ = index;
   return netType;
}

int ArmoryServersProvider::addKey(const QString &address, int port, const QString &key)
{
   // find server
   int index = -1;
   for (int i = 0; i < servers().size(); ++i) {
      if ((servers().at(i).armoryDBIp == address.toStdString())
         && (servers().at(i).armoryDBPort == std::to_string(port))) {
         index = i;
         break;
      }
   }

   if (index == -1){
      return -1;
   }

   if (index < ArmoryServersProvider::kDefaultServersCount) {
      return -1;
   }

   QStringList servers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   QString serverTxt = servers.at(index - ArmoryServersProvider::kDefaultServersCount);
   ArmoryServer server = ArmoryServer::fromTextSettings(serverTxt.toStdString());
   server.armoryDBKey = key.toStdString();
   servers[index - ArmoryServersProvider::kDefaultServersCount] = QString::fromStdString(server.toTextSettings());

   appSettings_->set(ApplicationSettings::armoryServers, servers);
   return index;
}

int ArmoryServersProvider::addKey(const std::string &srvIPPort, const BinaryData &srvPubKey)
{
   QString ipPort = QString::fromStdString(srvIPPort);
   QStringList ipPortList = ipPort.split(QStringLiteral(":"));
   if (ipPortList.size() == 2) {
      return addKey(ipPortList.at(0), ipPortList.at(1).toInt()
             , QString::fromStdString(srvPubKey.toHexStr()));
   }
   return -1;
}

void ArmoryServersProvider::setConnectedArmorySettings(const ArmorySettings &currentArmorySettings)
{
   lastConnectedIndex_ = indexOf(currentArmorySettings);
}

bool ArmoryServersProvider::isDefault(int index) const
{
   return index == 0 || index == 1;
}
