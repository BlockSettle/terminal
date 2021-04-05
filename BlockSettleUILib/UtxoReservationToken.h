/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef UTXO_RESERVATION_TOKEN_H
#define UTXO_RESERVATION_TOKEN_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <QObject>

namespace spdlog {
   class logger;
}

struct UTXO;

namespace bs {

   class UtxoReservationToken
   {
   public:
      // Make new (empty) reservation
      UtxoReservationToken();

      // Release reservation if it is valid
      ~UtxoReservationToken();

      UtxoReservationToken(const UtxoReservationToken&) = delete;
      UtxoReservationToken &operator=(const UtxoReservationToken&) = delete;

      // Move out reservation from other
      UtxoReservationToken(UtxoReservationToken &&other);

      // Move out reservation from other (and release current reservation if needed)
      UtxoReservationToken &operator=(UtxoReservationToken &&other);

      // Make new reservation (uses global UtxoReservationToken instance).
      // reserveId and walletId must be non-empty
      // logger could be nullptr
      static UtxoReservationToken makeNewReservation(const std::shared_ptr<spdlog::logger> &logger
         , const std::vector<UTXO> &utxos, const std::string &reserveId, std::function<void()>&& onReleasedCb);

      const std::string &reserveId() const { return reserveId_; }

      void release();

      bool isValid() const;

   private:
      // could be nullptr
      std::shared_ptr<spdlog::logger> logger_;
      std::function<void()> onReleasedCb_;
      std::string reserveId_;

   };

   struct FixedXbtInputs
   {
      // Need to use UTXO/walletId map for CC
      std::map<UTXO, std::string> inputs;
      UtxoReservationToken utxoRes;
   };

}  // namespace bs

#endif // UTXO_RESERVATION_TOKEN_H
