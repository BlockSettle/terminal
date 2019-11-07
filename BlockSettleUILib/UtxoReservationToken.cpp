#include "UtxoReservationToken.h"

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
   if (logger) {
      uint64_t sum = 0;
      for (const auto &utxo : utxos) {
         sum += utxo.getValue();
      }
      SPDLOG_LOGGER_DEBUG(logger, "make new UTXO reservation, walletId: {}, amount: {}, reserveId: {}", walletId, sum, reserveId);
   }

   UtxoReservationToken result;
   UtxoReservation::instance()->reserve(walletId, reserveId, utxos);
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
   if (reserveId_.empty()) {
      return;
   }
   if (logger_) {
      SPDLOG_LOGGER_DEBUG(logger_, "release UTXO reservation, reserveId: {}", reserveId_);
   }
   bs::UtxoReservation::instance()->unreserve(reserveId_);
   reserveId_.clear();
}
