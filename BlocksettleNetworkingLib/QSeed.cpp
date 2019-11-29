/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QFile>
#include <QVariant>
#include <QStandardPaths>

#include "WalletBackupFile.h"
#include "QSeed.h"

using namespace bs::wallet;

bs::wallet::QSeed::QNetworkType QSeed::toQNetworkType(NetworkType netType) { return static_cast<QNetworkType>(netType); }
NetworkType QSeed::fromQNetworkType(QNetworkType netType) { return static_cast<NetworkType>(netType); }


QSeed QSeed::fromPaperKey(const QString &key, QNetworkType netType)
{
   QSeed seed;
   try 
   {
      const auto seedLines = key.split(QLatin1String("\n"), QString::SkipEmptyParts);
      if (seedLines.count() == 2) 
      {
         EasyCoDec::Data easyData = { seedLines[0].toStdString(), seedLines[1].toStdString() };
         seed = bs::core::wallet::Seed::fromEasyCodeChecksum(easyData, fromQNetworkType(netType));
      }
      else 
      {
         throw std::runtime_error("invalid seed string line count");
      }
   }
   catch (const std::exception &e) {
      seed.lastError_ = tr("Failed to parse wallet key: %1").arg(QLatin1String(e.what()));
      throw std::runtime_error("unexpected seed string");
   }

   return seed;
}

QSeed QSeed::fromDigitalBackup(const QString &filename, QNetworkType netType)
{
   QSeed seed;

   QFile file(filename);
   if (!file.exists()) {
      seed.lastError_ = tr("Digital Backup file %1 doesn't exist").arg(filename);
      return seed;
   }
   if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      const auto wdb = WalletBackupFile::Deserialize(std::string(data.data(), data.size()));
      if (wdb.id.empty()) {
         seed.lastError_ = tr("Digital Backup file %1 corrupted").arg(filename);
      }
      else {
         seed = bs::core::wallet::Seed::fromEasyCodeChecksum(wdb.seed, fromQNetworkType(netType));
      }
   }
   else {
      seed.lastError_ = tr("Failed to read Digital Backup file %1").arg(filename);
   }

   return seed;
}

QString QSeed::lastError() const
{
   return lastError_;
}

