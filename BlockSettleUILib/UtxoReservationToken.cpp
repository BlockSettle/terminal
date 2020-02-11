/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UtxoReservationToken.h"

#include <cassert>
#include <spdlog/spdlog.h>

#include "UtxoReservation.h"
#include "UtxoReservationManager.h"

using namespace bs;

UtxoReservationToken::UtxoReservationToken() = default;

UtxoReservationToken::~UtxoReservationToken()
{
   release();
}

UtxoReservationToken::UtxoReservationToken(UtxoReservationToken &&other)
{
   *this = std::move(other);
}

UtxoReservationToken &UtxoReservationToken::operator=(UtxoReservationToken &&other)
{
   release();
   logger_ = other.logger_;
   reserveId_ = std::move(other.reserveId_);
   utxoReservantionManager_ = std::move(other.utxoReservantionManager_);
   other.reserveId_.clear();
   return *this;
}

UtxoReservationToken UtxoReservationToken::makeNewReservation(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::UTXOReservantionManager> &utxoReservantionManager
   , const std::vector<UTXO> &utxos, const std::string &reserveId)
{
   assert(!reserveId.empty());
   assert(UtxoReservation::instance());

   if (logger) {
      uint64_t sum = 0;
      for (const auto &utxo : utxos) {
         sum += utxo.getValue();
      }
      SPDLOG_LOGGER_DEBUG(logger, "make new UTXO reservation, amount: {}, reserveId: {}", sum, reserveId);
   }

   UtxoReservationToken result;
   UtxoReservation::instance()->reserve(reserveId, utxos);
   result.logger_ = logger;
   result.reserveId_ = reserveId;
   result.utxoReservantionManager_ = utxoReservantionManager;

   utxoReservantionManager->refreshAvailableUTXO();

   return result;
}

void UtxoReservationToken::release()
{
   if (!isValid()) {
      return;
   }

   if (!bs::UtxoReservation::instance()) {
      SPDLOG_LOGGER_ERROR(logger_, "UtxoReservation::instance is already destroyed");
      return;
   }

   bool result = bs::UtxoReservation::instance()->unreserve(reserveId_);
   if (!result && logger_) {
      SPDLOG_LOGGER_ERROR(logger_, "release UTXO reservation failed, reserveId: '{}'", reserveId_);
   }
   reserveId_.clear();
   utxoReservantionManager_->refreshAvailableUTXO();
}

bool UtxoReservationToken::isValid() const
{
   return !reserveId_.empty();
}
