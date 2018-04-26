#ifndef __CELER_CREATE_ORDER_H__
#define __CELER_CREATE_ORDER_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"
#include <memory>
#include <string>
#include <functional>

namespace spdlog
{
   class logger;
}

class CelerCreateOrderSequence : public CelerCommandSequence<CelerCreateOrderSequence>
{
public:
   CelerCreateOrderSequence(const std::string& accountName, const QString &reqId, const bs::network::Quote& quote, const std::string &payoutTx
      , const std::shared_ptr<spdlog::logger>& logger);
   ~CelerCreateOrderSequence() noexcept = default;

   CelerCreateOrderSequence(const CelerCreateOrderSequence&) = delete;
   CelerCreateOrderSequence& operator = (const CelerCreateOrderSequence&) = delete;

   CelerCreateOrderSequence(CelerCreateOrderSequence&&) = delete;
   CelerCreateOrderSequence& operator = (CelerCreateOrderSequence&&) = delete;

   bool FinishSequence() override;

private:
   CelerMessage createOrder();

   QString              reqId_;
   bs::network::Quote   quote_;
   std::string          payoutTx_;
   std::shared_ptr<spdlog::logger> logger_;
   const std::string accountName_;
};


class CelerFindAllOrdersSequence : public CelerCommandSequence<CelerFindAllOrdersSequence>
{
public:
   struct Message {
      CelerAPI::CelerMessageType type;
      std::string    payload;
   };
   using Messages = std::vector<Message>;
   using cbFinished = std::function<void(const CelerFindAllOrdersSequence::Messages &)>;

   explicit CelerFindAllOrdersSequence(const std::shared_ptr<spdlog::logger> &logger);
   ~CelerFindAllOrdersSequence() noexcept = default;

   CelerFindAllOrdersSequence(const CelerFindAllOrdersSequence&) = delete;
   CelerFindAllOrdersSequence& operator = (const CelerFindAllOrdersSequence&) = delete;
   CelerFindAllOrdersSequence(CelerFindAllOrdersSequence&&) = delete;
   CelerFindAllOrdersSequence& operator = (CelerFindAllOrdersSequence&&) = delete;

   bool FinishSequence() override;
   void SetCallback(const cbFinished &cb) { cbFinished_ = cb; }

private:
   CelerMessage create();
   bool process(const CelerMessage &);

   std::shared_ptr<spdlog::logger>  logger_;
   Messages    messages_;
   cbFinished  cbFinished_ = nullptr;
};

#endif // __CELER_CREATE_ORDER_H__
