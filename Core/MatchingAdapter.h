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

#include "Celer/BaseCelerClient.h"
#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Terminal {
      class AcceptRFQ;
      class MatchingMessage_Login;
      class PullRFQReply;
      class RFQ;
      class ReplyToRFQ;
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
   bool processGetSubmittedAuth(const bs::message::Envelope&);
   bool processSubmitAuth(const bs::message::Envelope&, const std::string& address);
   bool processSendRFQ(const BlockSettle::Terminal::RFQ&);
   bool processAcceptRFQ(const BlockSettle::Terminal::AcceptRFQ&);
   bool processCancelRFQ(const std::string& rfqId);
   bool processSubmitQuote(const BlockSettle::Terminal::ReplyToRFQ&);
   bool processPullQuote(const BlockSettle::Terminal::PullRFQReply&);

   std::string getQuoteReqId(const std::string& quoteId) const;
   void saveQuoteReqId(const std::string& quoteReqId, const std::string& quoteId);
   void delQuoteReqId(const std::string& quoteReqId);
   std::string getQuoteRequestCcy(const std::string& id) const;
   void saveQuoteRequestCcy(const std::string& id, const std::string& ccy);
   void cleanQuoteRequestCcy(const std::string& id);

   bool onQuoteResponse(const std::string&);
   bool onQuoteReject(const std::string&);
   bool onOrderReject(const std::string&);
   bool onBitcoinOrderSnapshot(const std::string&);
   bool onFxOrderSnapshot(const std::string&);
   bool onQuoteCancelled(const std::string&);
   bool onSignTxNotif(const std::string&);
   bool onQuoteAck(const std::string&);
   bool onQuoteReqNotification(const std::string&);
   bool onQuoteNotifCancelled(const std::string&);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_, userSettl_;
   std::unique_ptr<BaseCelerClient>    celerConnection_;

   std::string assignedAccount_;
   std::unordered_map<std::string, bs::network::RFQ>  submittedRFQs_;
   std::unordered_map<std::string, std::string>       quoteIdMap_;
   std::unordered_map<std::string, std::unordered_set<std::string>>  quoteIds_;
   std::unordered_map<std::string, std::string> quoteCcys_;
};


#endif	// MATCHING_ADAPTER_H
