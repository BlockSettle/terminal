/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ARMORY_SETTINGS_H__
#define __ARMORY_SETTINGS_H__

#include <string>
#include <QString>
#include <QStringList>

#include <bdmenums.h>
#include "BtcDefinitions.h"
#include "SecureBinaryData.h"


struct ArmoryServer
{
   QString name;
   NetworkType netType = NetworkType::Invalid;
   QString armoryDBIp;
   int armoryDBPort = 0;
   QString armoryDBKey;
   SecureBinaryData  password;
   bool runLocally = false;

   bool operator ==(const ArmoryServer &other) const  {
      return armoryDBIp == other.armoryDBIp
            && armoryDBPort == other.armoryDBPort
            && netType == other.netType;
   }
   bool operator !=(const ArmoryServer &other) const  {
      return !(*this == other);
   }
   static ArmoryServer fromTextSettings(const QString &text) {
      ArmoryServer server;
      const QStringList &data = text.split(QStringLiteral(":"));
      if (data.size() < 5) {  // password is optional now
         return server;
      }
      server.name = data.at(0);
      server.netType = data.at(1) == QStringLiteral("0") ? NetworkType::MainNet : NetworkType::TestNet;
      server.armoryDBIp = data.at(2);
      server.armoryDBPort = data.at(3).toInt();
      server.armoryDBKey = data.at(4);
      if (data.size() > 5) {
         server.password = data.at(5).toStdString();
      }
      return server;
   }

   QString toTextSettings() const {
      return QStringLiteral("%1:%2:%3:%4:%5:%6")
            .arg(name)
            .arg(netType == NetworkType::MainNet ? 0 : 1)
            .arg(armoryDBIp)
            .arg(armoryDBPort)
            .arg(armoryDBKey)
            .arg(QString::fromStdString(password.toBinStr()));
   }

   bool isValid() const {
      if (armoryDBPort < 1 || armoryDBPort > USHRT_MAX) {
         return false;
      }
      if (name.isEmpty()) {
         return false;
      }
      return true;
   }
};

struct ArmorySettings : public ArmoryServer
{
   SocketType socketType;

   QString armoryExecutablePath;
   QString dbDir;
   QString bitcoinBlocksDir;
   QString dataDir;
};

#endif // __ARMORY_SETTINGS_H__
