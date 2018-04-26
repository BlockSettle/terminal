#ifndef __CELER_SUBSCRIBE_TO_SECURITIES_H__
#define __CELER_SUBSCRIBE_TO_SECURITIES_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"
#include <functional>
#include <unordered_map>
#include <memory>

namespace spdlog
{
   class logger;
}

namespace bs {
   namespace network {

      class CelerSubscribeToSecurities : public CelerCommandSequence<CelerSubscribeToSecurities>
      {
      public:
         typedef std::unordered_map<std::string, bs::network::SecurityDef>   Securities;
         using onSecuritiesSnapshotReceived = std::function<void(const CelerSubscribeToSecurities::Securities &)>;

      public:
         CelerSubscribeToSecurities(const std::shared_ptr<spdlog::logger>& logger, const onSecuritiesSnapshotReceived &);
         ~CelerSubscribeToSecurities() noexcept = default;

         CelerSubscribeToSecurities(const CelerSubscribeToSecurities&) = delete;
         CelerSubscribeToSecurities& operator = (const CelerSubscribeToSecurities&) = delete;

         CelerSubscribeToSecurities(CelerSubscribeToSecurities&&) = delete;
         CelerSubscribeToSecurities& operator = (CelerSubscribeToSecurities&&) = delete;

         bool FinishSequence() override;

      private:
         CelerMessage subscribeFX();
         bool process(const CelerMessage& message);

      private:
         std::shared_ptr<spdlog::logger>     logger_;
         const onSecuritiesSnapshotReceived  onSnapshotReceived_;
         Securities                          dictionary_;
      };

   }  //namespace network
}  //namespace bs

#endif // __CELER_SUBSCRIBE_TO_SECURITIES_H__
