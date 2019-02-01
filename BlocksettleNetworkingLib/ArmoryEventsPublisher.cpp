#include "ArmoryEventsPublisher.h"

#include "ConnectionManager.h"
#include "PublisherConnection.h"

#include "armory_events.pb.h"

ArmoryEventsPublisher::ArmoryEventsPublisher(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::shared_ptr<spdlog::logger>& logger)
 : logger_{logger}
{
   publisher_ = connectionManager->CreatePublisherConnection();
}

ArmoryEventsPublisher::~ArmoryEventsPublisher() noexcept
{
   DisconnectFromArmoryConnection();
}

bool ArmoryEventsPublisher::IsConnectedToArmory() const
{
   return armoryConnection_ != nullptr;
}

void ArmoryEventsPublisher::DisconnectFromArmoryConnection()
{
   if (IsConnectedToArmory()) {
      logger_->debug("[ArmoryEventsPublisher::DisconnectFromArmoryConnection] disposing armory events publisher");

      disconnect(armoryConnection_.get(), &ArmoryConnection::newBlock, this, &ArmoryEventsPublisher::onNewBlock);
      disconnect(armoryConnection_.get(), &ArmoryConnection::zeroConfReceived, this, &ArmoryEventsPublisher::onZeroConfReceived);

      armoryConnection_ = nullptr;
      publisher_ = nullptr;
   }
}

bool ArmoryEventsPublisher::ConnectToArmoryConnection(const std::shared_ptr<ArmoryConnection>& armoryConnection)
{
   if (IsConnectedToArmory()) {
      logger_->debug("[ArmoryEventsPublisher::ConnectToArmoryConnection] already connected to armory");
      return false;
   }

   if (!publisher_->InitConnection()) {
      logger_->error("[ArmoryEventsPublisher::ConnectToArmoryConnection] failed to init publisher connection");
      return false;
   }

   if (!publisher_->BindPublishingConnection("armory_events")) {
      logger_->error("[ArmoryEventsPublisher::ConnectToArmoryConnection] failed to init internal publisher");
      return false;
   }

   armoryConnection_ = armoryConnection;

   // subscribe to all armory connection signals
   connect(armoryConnection.get(), &ArmoryConnection::newBlock
      , this, &ArmoryEventsPublisher::onNewBlock, Qt::QueuedConnection);
   connect(armoryConnection.get(), &ArmoryConnection::zeroConfReceived
      , this, &ArmoryEventsPublisher::onZeroConfReceived, Qt::QueuedConnection);

   return true;
}

void ArmoryEventsPublisher::onNewBlock(unsigned int height) const
{
   Blocksettle::ArmoryEvents::NewBlockEvent  eventData;

   eventData.set_height(height);

   Blocksettle::ArmoryEvents::EventHeader    header;

   header.set_event_type(Blocksettle::ArmoryEvents::NewBlockEventType);
   header.set_event_data(eventData.SerializeAsString());

   logger_->debug("[ArmoryEventsPublisher::onNewBlock] publishing event height {}"
      , height);

   if (!publisher_->PublishData(header.SerializeAsString())) {
      logger_->error("[ArmoryEventsPublisher::onNewBlock] failed to publish event");
   }
}

void ArmoryEventsPublisher::onZeroConfReceived(const std::vector<bs::TXEntry> entries) const
{
   Blocksettle::ArmoryEvents::ZCEvent  eventData;
   for (const auto &entry : entries) {
      auto zcEntry = eventData.add_zc_entries();
      zcEntry->set_tx_hash(entry.txHash.toBinStr());
      zcEntry->set_wallet_id(entry.id);
      zcEntry->set_value(entry.value);
      zcEntry->set_block_num(entry.blockNum);
      zcEntry->set_time(entry.txTime);
      zcEntry->set_rbf(entry.isRBF);
      zcEntry->set_chained_zc(entry.isChainedZC);
   }

   Blocksettle::ArmoryEvents::EventHeader    header;

   header.set_event_type(Blocksettle::ArmoryEvents::ZCEventType);
   header.set_event_data(eventData.SerializeAsString());

   logger_->debug("[ArmoryEventsPublisher::onZeroConfReceived] publishing ZC event");

   if (!publisher_->PublishData(header.SerializeAsString())) {
      logger_->error("[ArmoryEventsPublisher::onZeroConfReceived] failed to publish event");
   }
}