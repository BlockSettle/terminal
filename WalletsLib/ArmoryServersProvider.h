#ifndef ARMORY_SERVERS_PROVIDER_H
#define ARMORY_SERVERS_PROVIDER_H

#include <QObject>
#include "ApplicationSettings.h"
#include "BinaryData.h"


// Define the BIP 150 public keys used by servers controlled by BS. For dev
// purposes, they'll be hard-coded for now. THESE MUST BE REPLACED EVENTUALLY
// WITH THE KEY ROTATION ALGORITHM. HARD-CODED KEYS WILL KILL ANY TERMINAL ONCE
// THE KEYS ROTATE.
// armory.blocksettle.com - 185.213.153.37 server
#define TESTNET_ARMORY_BLOCKSETTLE_NAME "BlockSettle"
//#define TESTNET_ARMORY_BLOCKSETTLE_KEY "03a8649b32b9459961e143c5c111b9a47ffa494116791c1cb35945a8b9bc8254ab"
#define TESTNET_ARMORY_BLOCKSETTLE_ADDRESS "armory.blocksettle.com"
#define TESTNET_ARMORY_BLOCKSETTLE_PORT 81 //7681

#define MAINNET_ARMORY_BLOCKSETTLE_NAME "BlockSettle"
//#define MAINNET_ARMORY_BLOCKSETTLE_KEY "03a8649b32b9459961e143c5c111b9a47ffa494116791c1cb35945a8b9bc8254ab"
#define MAINNET_ARMORY_BLOCKSETTLE_ADDRESS "armory.blocksettle.com"
#define MAINNET_ARMORY_BLOCKSETTLE_PORT 80

class ArmoryServersProvider : public QObject
{
   Q_OBJECT
public:
   ArmoryServersProvider(const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent = nullptr);

   QList<ArmoryServer> servers() const;
   ArmorySettings getArmorySettings() const;

   int indexOfCurrent() const;   // index of server which set in ini file
   int indexOfConnected() const;   // index of server currently connected
   int indexOf(const QString &name) const;
   int indexOf(const ArmoryServer &server) const;
   int indexOfIpPort(const std::string &srvIPPort) const;

   bool add(const ArmoryServer &server);
   bool replace(int index, const ArmoryServer &server);
   bool remove(int index);
   void setupServer(int index);

   void addKey(const QString &address, int port, const QString &key);
   void addKey(const std::string &srvIPPort, const BinaryData &srvPubKey);

   static const int kDefaultServersCount;

   ArmorySettings connectedArmorySettings() const;
   void setConnectedArmorySettings(const ArmorySettings &connectedArmorySettings);

signals:
   void dataChanged();
private:
   std::shared_ptr<ApplicationSettings> appSettings_;
   static const QList<ArmoryServer> defaultServers_;

   ArmorySettings connectedArmorySettings_;  // latest connected server

};

#endif // ARMORY_SERVERS_PROVIDER_H
