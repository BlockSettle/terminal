/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef NETWORK_SETTINGS_LOADER_H
#define NETWORK_SETTINGS_LOADER_H

#include <spdlog/spdlog.h>
#include <memory>
#include <QObject>
#include "ZMQ_BIP15X_Helpers.h"

class RequestReplyCommand;

struct NetworkAddress {
   std::string host;
   int port{};
};

struct NetworkSettings {
   NetworkAddress  celer;
   NetworkAddress  marketData;
   NetworkAddress  mdhs;
   NetworkAddress  chat;
   NetworkAddress  proxy;
   bool            isSet = false;
};

class NetworkSettingsLoader : public QObject
{
   Q_OBJECT
public:
   NetworkSettingsLoader(const std::shared_ptr<spdlog::logger> &logger
      , const std::string &pubHost, const std::string &pubPort
      , const ZmqBipNewKeyCb &cbApprove, QObject *parent = nullptr);
   ~NetworkSettingsLoader() override;

   const NetworkSettings &settings() const { return networkSettings_; }

   // Do not call if network setting was already loaded!
   // It's normal to call multiple times if loading was started but is not ready yet.
   void loadSettings();

signals:
   void failed(const QString &errorMsg);
   void succeed();

private:
   void sendFailedAndReset(const QString &errorMsg);

   std::shared_ptr<spdlog::logger> logger_;

   const ZmqBipNewKeyCb cbApprove_;

   const std::string pubHost_;
   const std::string pubPort_;

   std::shared_ptr<RequestReplyCommand> cmdPuBSettings_;

   NetworkSettings networkSettings_;
};

#endif
