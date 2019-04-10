#ifndef __ARMORY_EVENTS_PUBLISHER_H__
#define __ARMORY_EVENTS_PUBLISHER_H__

#include "ArmoryObject.h"

#include <memory>
#include <string>

#include <spdlog/logger.h>

#include <QObject>
#include <QString>

class ConnectionManager;
class PublisherConnection;

// connects to armory signals and broadcast it to inproc zmq connection
// events can be received with ArmoryEventsSubscriber
class ArmoryEventsPublisher : public QObject
{
Q_OBJECT
public:
   ArmoryEventsPublisher(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::shared_ptr<spdlog::logger>& logger);
   ~ArmoryEventsPublisher() noexcept override;

   ArmoryEventsPublisher(const ArmoryEventsPublisher&) = delete;
   ArmoryEventsPublisher& operator = (const ArmoryEventsPublisher&) = delete;

   ArmoryEventsPublisher(ArmoryEventsPublisher&&) = delete;
   ArmoryEventsPublisher& operator = (ArmoryEventsPublisher&&) = delete;

   bool connectToArmory(const std::shared_ptr<ArmoryObject>& armoryConnection);
   void disconnectFromArmory();

private:
   bool isConnectedToArmory() const;

private slots:
   void onNewBlock(unsigned int height) const;
   void onZeroConfReceived(const std::vector<bs::TXEntry>) const;

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<PublisherConnection>   publisher_;
   std::shared_ptr<ArmoryObject>          armory_ = nullptr;
};

#endif // __ARMORY_EVENTS_PUBLISHER_H__