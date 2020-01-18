/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UtxoReservationToken.h"

#include <cassert>
#include <spdlog/spdlog.h>

#include "CoreWallet.h"
#include "UtxoReservation.h"

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
   other.reserveId_.clear();
   return *this;
}

UtxoReservationToken UtxoReservationToken::makeNewReservation(const std::shared_ptr<spdlog::logger> &logger
   , const std::vector<UTXO> &utxos, const std::string &reserveId, const std::string &walletId)
{
   assert(!reserveId.empty());
   assert(!walletId.empty());
   assert(UtxoReservation::instance());

   if (logger) {
      uint64_t sum = 0;
      for (const auto &utxo : utxos) {
         sum += utxo.getValue();
      }
      SPDLOG_LOGGER_DEBUG(logger, "make new UTXO reservation, walletId: {}, amount: {}, reserveId: {}", walletId, sum, reserveId);
   }

   UtxoReservationToken result;
   UtxoReservation::instance()->reserve(reserveId, utxos);
   result.logger_ = logger;
   result.reserveId_ = reserveId;
   return result;
}

UtxoReservationToken UtxoReservationToken::makeNewReservation(const std::shared_ptr<spdlog::logger> &logger, const core::wallet::TXSignRequest &txReq, const std::string &reserveId)
{
   return makeNewReservation(logger, txReq.inputs, reserveId, txReq.walletIds.front());
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
}

bool UtxoReservationToken::isValid() const
{
   return !reserveId_.empty();
}
