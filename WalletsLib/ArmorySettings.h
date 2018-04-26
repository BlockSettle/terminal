#ifndef __ARMORY_SETTINGS_H__
#define __ARMORY_SETTINGS_H__

#include <string>
#include <QString>

#include <bdmenums.h>
#include "BtcDefinitions.h"


struct ArmorySettings
{
   NetworkType netType;

   bool ignoreAllZC;

   SocketType socketType;

   std::string armoryDBIp;
   std::string armoryDBPort;

   bool runLocally;

   QString armoryExecutablePath;
   QString dbDir;
   QString bitcoinBlocksDir;
};

#endif // __ARMORY_SETTINGS_H__
