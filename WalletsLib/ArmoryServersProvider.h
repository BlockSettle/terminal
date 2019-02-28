#ifndef ARMORY_SERVERS_PROVIDER_H
#define ARMORY_SERVERS_PROVIDER_H

#include <QObject>
#include "ApplicationSettings.h"
#include "BinaryData.h"

class ArmoryServersProvider : public QObject
{
   Q_OBJECT
public:
   ArmoryServersProvider(const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent = nullptr);

   QList<ArmoryServer> servers() const;
   ArmorySettings getArmorySettings() const;
   int indexOf(const QString &name) const;
   int indexOfIpPort(const std::string &srvIPPort) const;
   void add(const ArmoryServer &server);
   void remove(int index);
   void setupServer(int index);

   void addKey(const QString &address, int port, const QString &key);
   void addKey(const std::string &srvIPPort, const BinaryData &srvPubKey);

   static const int defaultServersCount = 4;

signals:
   void dataChanged();
private:
   std::shared_ptr<ApplicationSettings> appSettings_;
};

#endif // ARMORY_SERVERS_PROVIDER_H
