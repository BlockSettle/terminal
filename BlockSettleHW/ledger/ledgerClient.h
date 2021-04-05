/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef LEDGERCLIENT_H
#define LEDGERCLIENT_H

#include "ledgerStructure.h"

#include <memory>
#include <mutex>

#include <QVector>

class LedgerDevice;
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

class LedgerClient : public QObject
{
   Q_OBJECT
public:
   LedgerClient(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<bs::sync::WalletsManager> walletManager, bool testNet, QObject *parent = nullptr);
   ~LedgerClient() override = default;

   void scanDevices(AsyncCallBack&& cb);

   QVector<DeviceKey> deviceKeys() const;

   QPointer<LedgerDevice> getDevice(const QString& deviceId);

   QString lastScanError() const;

private:
   QVector<QPointer<LedgerDevice>> availableDevices_;
   bool testNet_;
   QString lastScanError_;

   std::shared_ptr<spdlog::logger>           logger_;
   std::shared_ptr<bs::sync::WalletsManager> walletManager_;
   std::shared_ptr<std::mutex>               hidLock_;

};

#endif // LEDGERCLIENT_H
