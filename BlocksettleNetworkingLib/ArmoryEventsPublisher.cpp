/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   disconnectFromArmory();
}

bool ArmoryEventsPublisher::isConnectedToArmory() const
{
   return act_ != nullptr;
}

void ArmoryEventsPublisher::disconnectFromArmory()
{
   if (isConnectedToArmory()) {
      logger_->debug("[ArmoryEventsPublisher::DisconnectFromArmoryConnection] disposing armory events publisher");

      act_.reset();
      publisher_ = nullptr;
   }
}

bool ArmoryEventsPublisher::connectToArmory(const std::shared_ptr<ArmoryConnection> &armoryConnection)
{
   if (isConnectedToArmory()) {
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

   act_ = make_unique<PublisherACT>(this);
   act_->init(armoryConnection.get());
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

void ArmoryEventsPublisher::onZeroConfReceived(const std::vector<bs::TXEntry> &entries) const
{
   Blocksettle::ArmoryEvents::ZCEvent  eventData;
   for (const auto &entry : entries) {
      auto zcEntry = eventData.add_zc_entries();
      zcEntry->set_tx_hash(entry.txHash.toBinStr());
      zcEntry->set_wallet_id(*entry.walletIds.cbegin());
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
