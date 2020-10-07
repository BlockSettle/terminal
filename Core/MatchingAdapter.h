/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef MATCHING_ADAPTER_H
#define MATCHING_ADAPTER_H

#include "BaseCelerClient.h"
#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Terminal {
      class MatchingMessage_Login;
   }
}

class MatchingAdapter;
class ClientCelerConnection : public BaseCelerClient
{
public:
   ClientCelerConnection(const std::shared_ptr<spdlog::logger>& logger
      , MatchingAdapter* parent, bool userIdRequired, bool useRecvTimer);
   ~ClientCelerConnection() noexcept override = default;

protected:
   void onSendData(CelerAPI::CelerMessageType messageType, const std::string& data) override;

private:
   MatchingAdapter* parent_{ nullptr };
};


class MatchingAdapter : public bs::message::Adapter, public CelerCallbackTarget
{
   friend class ClientCelerConnection;
public:
   MatchingAdapter(const std::shared_ptr<spdlog::logger> &);
   ~MatchingAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Matching"; }

private:
   // CelerCallbackTarget overrides
   void connectedToServer() override;
   void connectionClosed() override;
   void connectionError(int errorCode) override;

   bool processLogin(const BlockSettle::Terminal::MatchingMessage_Login&);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_;
   std::unique_ptr<BaseCelerClient>    celerConnection_;
};


#endif	// MATCHING_ADAPTER_H
