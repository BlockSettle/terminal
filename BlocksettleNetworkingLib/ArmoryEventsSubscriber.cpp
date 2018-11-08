#include "ArmoryEventsSubscriber.h"

#include "ConnectionManager.h"

#include "armory_events.pb.h"

ArmoryEventsSubscriber::ArmoryEventsSubscriber(const std::string& name
      , const std::shared_ptr<spdlog::logger>& logger)
 : name_{name}
 , logger_{logger}
{}

ArmoryEventsSubscriber::~ArmoryEventsSubscriber()
{
   UnsubscribeFromEvents();
}

void ArmoryEventsSubscriber::SetNewBlockCallback(const onNewBlockCB& cb)
{
   onNewBlock_ = cb;
}

void ArmoryEventsSubscriber::SetZCCallback(const onZCEventCB& cb)
{
   onZCEvent_ = cb;
}

bool ArmoryEventsSubscriber::SubscribeToArmoryEvents(const std::shared_ptr<ConnectionManager>& connectionManager)
{
   logger_->debug("[ArmoryEventsSubscriber::SubscribeToArmoryEvents] start subscription for {}"
      , name_);

   if (subConection_ != nullptr) {
      logger_->error("[ArmoryEventsSubscriber::SubscribeToArmoryEvents] {} :subscription already active"
         , name_);
      return false;
   }

   subConection_ = connectionManager->CreateSubscriberConnection();
   if (!subConection_->ConnectToPublisher("armory_events", this)) {
      logger_->error("[ArmoryEventsSubscriber::SubscribeToArmoryEvents] {} : failed to subscribe"
         , name_);
      subConection_ = nullptr;
      return false;
   }

   return true;
}

void ArmoryEventsSubscriber::OnDataReceived(const std::string& data)
{}

void ArmoryEventsSubscriber::OnConnected()
{
   logger_->debug("[ArmoryEventsSubscriber::OnConnected] {} connected to armory events"
      , name_);
}

void ArmoryEventsSubscriber::OnDisconnected()
{
   logger_->debug("[ArmoryEventsSubscriber::OnDisconnected] {} disconnected to armory events"
      , name_);
}
