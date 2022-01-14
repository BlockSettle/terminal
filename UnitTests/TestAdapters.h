/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TEST_ADAPTERS_H
#define TEST_ADAPTERS_H

#include <future>
#include <memory>
#include <unordered_map>
#include "CommonTypes.h"
#include "Message/Adapter.h"
#include "TerminalMessage.h"

namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Terminal {
      class BsServerMessage;
      class BsServerMessage_SignXbtHalf;
      class BsServerMessage_XbtTransaction;
      class MatchingMessage;
      class RFQ;
      class ReplyToRFQ;
   }
}
struct ArmoryInstance;

class TestSupervisor : public bs::message::Adapter
{
public:
   TestSupervisor(const std::string& name) : name_(name)
   {}

   bool process(const bs::message::Envelope &) override;

   bool processBroadcast(const bs::message::Envelope& env) override
   {
      return process(env);
   }

   bs::message::Adapter::Users supportedReceivers() const override
   {
      return { std::make_shared<bs::message::UserSupervisor>() };
   }
   std::string name() const override { return "sup" + name_; }

   bs::message::SeqId send(bs::message::TerminalUsers sender, bs::message::TerminalUsers receiver
      , const std::string &message, bs::message::SeqId respId = 0);
   bool pushFill(bs::message::Envelope &env) { return bs::message::Adapter::pushFill(env); }

   using FilterCb = std::function<bool(const bs::message::Envelope &)>;
   std::future<std::vector<bs::message::Envelope>> waitFor(const FilterCb &, size_t count
      , uint32_t *seqNo = nullptr);
   std::future<bs::message::Envelope> waitFor(const FilterCb &, uint32_t *seqNo = nullptr);
   void unwaitFor(const uint32_t seqNo);

private:
   std::string name_;
   std::map<uint32_t, std::pair<size_t, std::vector<bs::message::Envelope>>>  filterMap_;
   std::map<uint32_t, std::pair<FilterCb, std::shared_ptr<std::promise<std::vector<bs::message::Envelope>>>>> filterMultWait_;
   std::map<uint32_t, std::pair<FilterCb, std::shared_ptr<std::promise<bs::message::Envelope>>>>   filterWait_;
   std::mutex  mtxWait_;
   std::atomic_uint32_t seqNo_{ 0 };
};


class MatchingMock : public bs::message::Adapter
{
public:
   MatchingMock(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& name, const std::string& email
      , const std::shared_ptr<ArmoryInstance> &);  // for pushing ZCs (mocking PB)

   bool process(const bs::message::Envelope&) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   bs::message::Adapter::Users supportedReceivers() const override
   {
      return { user_, userBS_ };
   }
   std::string name() const override { return "Match" + name_; }

   void link(const std::shared_ptr<MatchingMock>&);
   bool inject(const BlockSettle::Terminal::MatchingMessage&, const std::string& email);
   bool inject(const BlockSettle::Terminal::BsServerMessage&, const std::string& email);

private:
   bool sendIncomingRFQ(const BlockSettle::Terminal::RFQ&, const std::string &email);
   bool sendQuoteReply(const BlockSettle::Terminal::ReplyToRFQ&, const std::string& email);
   bool sendQuote(const bs::network::Quote&);
   bool sendPendingOrder(const std::string& rfqId);
   bool sendFilledOrder(const std::string& rfqId);

   bool sendUnsignedPayinRequest(const std::string& settlIdBin);
   bool sendSignedPayinRequest(const BlockSettle::Terminal::BsServerMessage_SignXbtHalf&);
   bool sendSignedPayoutRequest(const BlockSettle::Terminal::BsServerMessage_SignXbtHalf&);
   bool processUnsignedPayin(const BlockSettle::Terminal::BsServerMessage_XbtTransaction&);
   bool processSignedTX(const BlockSettle::Terminal::BsServerMessage_XbtTransaction&
      , bool payin, bool recurse = false);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_, userBS_, userSettl_;
   std::string name_, email_;
   std::shared_ptr<ArmoryInstance>  armoryInst_;

   std::set<std::shared_ptr<MatchingMock>>   siblings_;

   struct Match {
      std::string sesToken;
      std::string reqAuthPubKey;
      bs::network::Quote   quote;
      bs::network::Order   order;
      BinaryData  signedPayin;
      BinaryData  signedPayout;

      bool isSellXBT() const;
   };
   std::unordered_map<std::string, Match> matches_;
};
#endif // TEST_ADAPTERS_H
