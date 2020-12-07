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
      class MatchingMessage;
      class RFQ;
      class ReplyToRFQ;
   }
}

class TestSupervisor : public bs::message::Adapter
{
public:
   TestSupervisor(const std::string& name) : name_(name)
   {}
   bool process(const bs::message::Envelope &) override;

   bs::message::Adapter::Users supportedReceivers() const override
   {
      return { std::make_shared<bs::message::UserSupervisor>() };
   }
   std::string name() const override { return "sup" + name_; }

   uint64_t send(bs::message::TerminalUsers sender, bs::message::TerminalUsers receiver
      , const std::string &message, bool request = false);
   bool push(const bs::message::Envelope &env) { return bs::message::Adapter::push(env); }
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
      , const std::string& name, const std::string& email);

   bool process(const bs::message::Envelope&) override;

   bs::message::Adapter::Users supportedReceivers() const override
   {
      return { user_ };
   }
   std::string name() const override { return "Match" + name_; }

   void link(const std::shared_ptr<MatchingMock>&);
   bool inject(const BlockSettle::Terminal::MatchingMessage&, const std::string &email);

private:
   bool sendIncomingRFQ(const BlockSettle::Terminal::RFQ&, const std::string &email);
   bool sendQuoteReply(const BlockSettle::Terminal::ReplyToRFQ&, const std::string& email);
   bool sendQuote(const bs::network::Quote&);
   bool sendPendingOrder(const std::string& rfqId);
   bool sendFilledOrder(const std::string& rfqId);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_, userSettl_;
   std::string name_, email_;

   std::set<std::shared_ptr<MatchingMock>>   siblings_;

   struct Match {
      std::string sesToken;
      bs::network::Quote   quote;
      bs::network::Order   order;
   };
   std::unordered_map<std::string, Match> matches_;
};
#endif // TEST_ADAPTERS_H
