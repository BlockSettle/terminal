#ifndef __ARMORY_EVENTS_PUBLISHER_H__
#define __ARMORY_EVENTS_PUBLISHER_H__

#include "ArmoryConnection.h"

#include <memory>
#include <string>

#include <spdlog/logger.h>


class ConnectionManager;
class PublisherConnection;

// connects to armory signals and broadcast it to inproc zmq connection
// events can be received with ArmoryEventsSubscriber
class ArmoryEventsPublisher
{
public:
   ArmoryEventsPublisher(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::shared_ptr<spdlog::logger>& logger);
   ~ArmoryEventsPublisher() noexcept;

   ArmoryEventsPublisher(const ArmoryEventsPublisher&) = delete;
   ArmoryEventsPublisher& operator = (const ArmoryEventsPublisher&) = delete;

   ArmoryEventsPublisher(ArmoryEventsPublisher&&) = delete;
   ArmoryEventsPublisher& operator = (ArmoryEventsPublisher&&) = delete;

   bool connectToArmory(const std::shared_ptr<ArmoryConnection> &);
   void disconnectFromArmory();

   bool isConnectedToArmory() const;

private:
   void onNewBlock(unsigned int height) const;
   void onZeroConfReceived(const std::vector<bs::TXEntry> &) const;

   class PublisherACT : public ArmoryCallbackTarget
   {
   public:
      PublisherACT(ArmoryEventsPublisher *parent)
         : parent_(parent) {}
      ~PublisherACT() override { cleanup(); }
      void onNewBlock(unsigned int height, unsigned int) override {
         parent_->onNewBlock(height);
      }
      void onZCReceived(const std::vector<bs::TXEntry> &zcs) override {
         parent_->onZeroConfReceived(zcs);
      }
   private:
      const ArmoryEventsPublisher *parent_;
   };

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<PublisherConnection>   publisher_;
   std::unique_ptr<PublisherACT>          act_;
};

#endif // __ARMORY_EVENTS_PUBLISHER_H__
