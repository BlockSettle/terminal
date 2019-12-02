/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "NetworkSettingsLoader.h"

#include "RequestReplyCommand.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "bs_communication.pb.h"

NetworkSettingsLoader::NetworkSettingsLoader(const std::shared_ptr<spdlog::logger> &logger
   , const std::string &pubHost, const std::string &pubPort
   , const ZmqBipNewKeyCb &cbApprove, QObject *parent)
   : QObject (parent)
   , logger_(logger)
   , cbApprove_(cbApprove)
   , pubHost_(pubHost)
   , pubPort_(pubPort)
{
}

void NetworkSettingsLoader::loadSettings()
{
   assert(!networkSettings_.isSet);
   if (cmdPuBSettings_) {
      // Loading is in progress
      return;
   }

   ZmqBIP15XDataConnectionParams params;
   params.ephemeralPeers = true;
   auto connection = std::make_shared<ZmqBIP15XDataConnection>(logger_, params);
   connection->setCBs(cbApprove_);

   Blocksettle::Communication::RequestPacket reqPkt;
   reqPkt.set_requesttype(Blocksettle::Communication::GetNetworkSettingsType);
   reqPkt.set_requestdata("");

   cmdPuBSettings_ = std::make_shared<RequestReplyCommand>("network_settings", connection, logger_);

   cmdPuBSettings_->SetReplyCallback([this](const std::string &data) {
      QMetaObject::invokeMethod(this, [this, data] {
         if (data.empty()) {
            sendFailedAndReset(tr("Empty reply from BlockSettle server"));
            return;
         }

         cmdPuBSettings_->resetConnection();

         Blocksettle::Communication::GetNetworkSettingsResponse response;
         if (!response.ParseFromString(data)) {
            sendFailedAndReset(tr("Invalid reply from BlockSettle server"));
            return;
         }

         if (!response.has_celer()) {
            sendFailedAndReset(tr("Missing Celer connection settings"));
            return;
         }

         if (!response.has_marketdata()) {
            sendFailedAndReset(tr("Missing MD connection settings"));
            return;
         }

         if (!response.has_mdhs()) {
            sendFailedAndReset(tr("Missing MDHS connection settings"));
            return;
         }

         if (!response.has_chat()) {
            sendFailedAndReset(tr("Missing Chat connection settings"));
            return;
         }

         networkSettings_.celer = { response.celer().host(), int(response.celer().port()) };
         networkSettings_.marketData = { response.marketdata().host(), int(response.marketdata().port()) };
         networkSettings_.mdhs = { response.mdhs().host(), int(response.mdhs().port()) };
         networkSettings_.chat = { response.chat().host(), int(response.chat().port()) };
         networkSettings_.proxy = { response.proxy().host(), int(response.proxy().port()) };
         networkSettings_.isSet = true;

         cmdPuBSettings_.reset();
         emit succeed();
      });

      return true;
   });

   cmdPuBSettings_->SetErrorCallback([this](const std::string& message) {
      SPDLOG_LOGGER_ERROR(logger_, "networking settings load failed: {}", message);
      QMetaObject::invokeMethod(this, [this] {
         sendFailedAndReset(tr("Failed to obtain network settings from BlockSettle server"));
      });
   });

   const bool executeOnConnect = true;
   bool result = cmdPuBSettings_->ExecuteRequest(pubHost_, pubPort_
      , reqPkt.SerializeAsString(), executeOnConnect);

   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "failed to send request");
      sendFailedAndReset(tr("Failed to retrieve network settings due to invalid connection to BlockSettle server"));
   }
}

NetworkSettingsLoader::~NetworkSettingsLoader() = default;

void NetworkSettingsLoader::sendFailedAndReset(const QString &errorMsg)
{
   cmdPuBSettings_.reset();
   emit failed(errorMsg);
}
