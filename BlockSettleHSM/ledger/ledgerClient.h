/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef LEDGERCLIENT_H
#define LEDGERCLIENT_H

#include "ledgerStructure.h"

#include <QVector>

class LedgerDevice;
namespace spdlog {
   class logger;
}

class LedgerClient : public QObject
{
   Q_OBJECT
public:
   LedgerClient(std::shared_ptr<spdlog::logger> logger, bool testNet, QObject *parent = nullptr);
   ~LedgerClient() override = default;

   void scanDevices();

   QVector<DeviceKey> deviceKeys() const;

   QPointer<LedgerDevice> getDevice(const QString& deviceId);

private:
   QVector<QPointer<LedgerDevice>> availableDevices_;
   bool testNet_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // LEDGERCLIENT_H
