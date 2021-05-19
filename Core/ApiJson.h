/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef API_JSON_H
#define API_JSON_H

#include <chrono>
#include <set>
#include "Address.h"
#include "ApiAdapter.h"
#include "ServerConnection.h"
#include "ServerConnectionListener.h"
#include "SignContainer.h"
#include "WsConnection.h"

namespace BlockSettle {
   namespace Terminal {
      class SettingsMessage_SettingsResponse;
   }
}

class ApiJsonAdapter : public ApiBusAdapter, public ServerConnectionListener
{
public:
   ApiJsonAdapter(const std::shared_ptr<spdlog::logger> &);
   ~ApiJsonAdapter() override = default;

   bool process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "JSON API"; }

protected:  // ServerConnectionListener overrides
   void OnDataFromClient(const std::string& clientId, const std::string& data) override;
   void OnClientConnected(const std::string& clientId, const Details& details) override;
   void OnClientDisconnected(const std::string& clientId) override;

private:
   bool processSettings(const bs::message::Envelope &);
   bool processSettingsGetResponse(const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);

   bool processAdminMessage(const bs::message::Envelope &);
   bool processAssets(const bs::message::Envelope&);
   bool processBlockchain(const bs::message::Envelope&);
   bool processBsServer(const bs::message::Envelope&);
   bool processMatching(const bs::message::Envelope&);
   bool processMktData(const bs::message::Envelope&);
   bool processOnChainTrack(const bs::message::Envelope&);
   bool processSettlement(const bs::message::Envelope&);
   bool processSigner(const bs::message::Envelope&);
   bool processWallets(const bs::message::Envelope&);

   void processStart();

   bool hasRequest(uint64_t msgId) const;
   bool sendReplyToClient(uint64_t msgId, const google::protobuf::Message&
      , const std::shared_ptr<bs::message::User> &sender);

   void sendGCtimeout();
   void processGCtimeout();

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<bs::message::UserTerminal>   userSettings_;
   std::unique_ptr<ServerConnection>      connection_;
   bs::network::ws::PrivateKey            connectionPrivKey_;
   std::set<std::string>   clientPubKeys_;
   std::set<std::string>   connectedClients_;

   std::string loggedInUser_;
   bool        matchingConnected_{ false };
   int         armoryState_{ -1 };
   uint32_t    blockNum_{ 0 };
   int         signerState_{ -1 };
   bool        walletsReady_{ false };

   struct ClientRequest
   {
      std::string clientId;
      std::string requestId;
      std::chrono::system_clock::time_point timestamp;
      bool replied{ false };
   };
   std::map<uint64_t, ClientRequest>   requests_;
};


#endif	// API_JSON_H
