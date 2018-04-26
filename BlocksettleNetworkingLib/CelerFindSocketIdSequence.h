#ifndef __CELER_FIND_SOCKET_ID_SEQUENCE_H__
#define __CELER_FIND_SOCKET_ID_SEQUENCE_H__

#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}

#include "CelerCommandSequence.h"

class CelerFindSocketIdSequence : public CelerCommandSequence<CelerFindSocketIdSequence>
{
public:
   using callback_func = std::function<void (int64_t socketId)>;
public:
   CelerFindSocketIdSequence(const std::shared_ptr<spdlog::logger>& logger, uint16_t port);
   ~CelerFindSocketIdSequence() noexcept override = default;

   CelerFindSocketIdSequence(const CelerFindSocketIdSequence&) = delete;
   CelerFindSocketIdSequence& operator = (const CelerFindSocketIdSequence&) = delete;

   CelerFindSocketIdSequence(CelerFindSocketIdSequence&&) = delete;
   CelerFindSocketIdSequence& operator = (CelerFindSocketIdSequence&&) = delete;

   void SetCallback(const callback_func& callback);
   bool FinishSequence() override;

private:
   CelerMessage sendFindAllSocketsRequest();
   bool processFindSocketsResponse(const CelerMessage& message);

private:
   std::shared_ptr<spdlog::logger> logger_;
   uint16_t port_;
   callback_func callback_;

   int64_t socketId_;
};

#endif // __CELER_FIND_SOCKET_ID_SEQUENCE_H__
