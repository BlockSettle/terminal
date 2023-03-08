/*

***********************************************************************************
* Copyright (C) 2019 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ARMORY_SERVERS_PROVIDER_H
#define ARMORY_SERVERS_PROVIDER_H

#include "ApplicationSettings.h"
#include "BinaryData.h"

#define ARMORY_BLOCKSETTLE_NAME "BlockSettle"

#define MAINNET_ARMORY_BLOCKSETTLE_ADDRESS "armory.blocksettle.com"
#define MAINNET_ARMORY_BLOCKSETTLE_PORT 80

#define TESTNET_ARMORY_BLOCKSETTLE_ADDRESS "armory-testnet.blocksettle.com"
#define TESTNET_ARMORY_BLOCKSETTLE_PORT 80

class ArmoryServersProvider
{
public:
   ArmoryServersProvider(const std::shared_ptr<ApplicationSettings>&);

   std::vector<ArmoryServer> servers() const;
   ArmorySettings getArmorySettings() const;

   int indexOfCurrent() const;   // index of server which set in ini file
   int indexOfConnected() const;   // index of server currently connected
   int indexOf(const QString &name) const;
   int indexOf(const ArmoryServer &server) const;
   int indexOfIpPort(const std::string &srvIPPort) const;
   static int getIndexOfMainNetServer();
   static int getIndexOfTestNetServer();

   bool add(const ArmoryServer &server);
   bool replace(int index, const ArmoryServer &server);
   bool remove(int index);
   void setupServer(int index);

   int addKey(const QString &address, int port, const QString &key);
   int addKey(const std::string &srvIPPort, const BinaryData &srvPubKey);

   void setConnectedArmorySettings(const ArmorySettings &);

   // if default armory used
   bool isDefault(int index) const;
   static const int kDefaultServersCount;

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;
   int lastConnectedIndex_{ -1 };
   static const std::vector<ArmoryServer> defaultServers_;
};

#endif // ARMORY_SERVERS_PROVIDER_H
