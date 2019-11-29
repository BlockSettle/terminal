/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RfqStorage.h"

#include "SettlementContainer.h"

RfqStorage::RfqStorage() = default;

RfqStorage::~RfqStorage() = default;

void RfqStorage::addSettlementContainer(std::shared_ptr<bs::SettlementContainer> rfq)
{
   const auto id = rfq->id();

   auto deleteCb = [this, handle = validityFlag_.handle(), id] {
      if (!handle.isValid()) {
         return;
      }
      auto it = rfqs_.find(id);
      if (it == rfqs_.end()) {
         return;
      }
      it->second->deactivate();
      rfqs_.erase(it);
   };

   // Use QueuedConnection so SettlementContainer is destroyed later
   QObject::connect(rfq.get(), &bs::SettlementContainer::completed, this, deleteCb, Qt::QueuedConnection);
   QObject::connect(rfq.get(), &bs::SettlementContainer::failed, this, deleteCb, Qt::QueuedConnection);
   QObject::connect(rfq.get(), &bs::SettlementContainer::timerExpired, this, deleteCb, Qt::QueuedConnection);

   rfqs_[rfq->id()] = std::move(rfq);
}

bs::SettlementContainer *RfqStorage::settlementContainer(const std::string &id) const
{
   auto it = rfqs_.find(id);
   if (it == rfqs_.end()) {
      return nullptr;
   }
   return it->second.get();
}
