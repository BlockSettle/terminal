/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ARMORY_EVENTS_SUBSCRIBER_H__
#define __ARMORY_EVENTS_SUBSCRIBER_H__

#include "SubscriberConnection.h"

#include <functional>
#include <memory>
#include <string>

#include <spdlog/logger.h>

#include "ArmoryConnection.h"

class ConnectionManager;

class ArmoryEventsSubscriber : public SubscriberConnectionListener
{
public:
   using onNewBlockCB = std::function<void (unsigned int32_t)>;
   using onZCEventCB = std::function<void (const std::vector<bs::TXEntry>)>;

public:
   ArmoryEventsSubscriber(const std::string& name
      , const std::shared_ptr<spdlog::logger>& logger);
   ~ArmoryEventsSubscriber() noexcept override;

   ArmoryEventsSubscriber(const ArmoryEventsSubscriber&) = delete;
   ArmoryEventsSubscriber& operator = (const ArmoryEventsSubscriber&) = delete;

   ArmoryEventsSubscriber(ArmoryEventsSubscriber&&) = delete;
   ArmoryEventsSubscriber& operator = (ArmoryEventsSubscriber&&) = delete;

   void SetNewBlockCallback(const onNewBlockCB& cb);
   void SetZCCallback(const onZCEventCB& cb);

   bool SubscribeToArmoryEvents(const std::shared_ptr<ConnectionManager>& connectionManager);

private:
   void UnsubscribeFromEvents();

   void ProcessNewBlockEvent(const std::string& eventData);
   void ProcessZCEvent(const std::string& eventData);

public:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;

private:
   const std::string                name_;
   std::shared_ptr<spdlog::logger>  logger_;

   onNewBlockCB                     onNewBlock_;
   onZCEventCB                      onZCEvent_;

   std::shared_ptr<SubscriberConnection> subConection_ = nullptr;
};

#endif // __ARMORY_EVENTS_SUBSCRIBER_H__