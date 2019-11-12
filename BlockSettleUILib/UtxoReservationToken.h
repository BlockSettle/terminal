#ifndef UTXO_RESERVATION_TOKEN_H
#define UTXO_RESERVATION_TOKEN_H

#include <memory>
#include <vector>

namespace spdlog {
   class logger;
}

namespace bs {
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }
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
      UtxoReservationToken &operator = (const UtxoReservationToken&) = delete;

      // Move out reservation from other
      UtxoReservationToken(UtxoReservationToken &&other);

      // Move out reservation from other (and release current reservation if needed)
      UtxoReservationToken &operator=(UtxoReservationToken &&other);

      // Make new reservation (uses global UtxoReservationToken instance).
      // reserveId and walletId must be non-empty
      // logger could be nullptr
      static UtxoReservationToken makeNewReservation(const std::shared_ptr<spdlog::logger> &logger
         , const std::vector<UTXO> &utxos
         , const std::string &reserveId
         , const std::string &walletId);

      static UtxoReservationToken makeNewReservation(const std::shared_ptr<spdlog::logger> &logger
         , const bs::core::wallet::TXSignRequest &txReq
         , const std::string &reserveId);

      const std::string &reserveId() const { return reserveId_; }

      void release();

   private:
      // could be nullptr
      std::shared_ptr<spdlog::logger> logger_;
      std::string reserveId_;

   };

}  // namespace bs

#endif // UTXO_RESERVATION_TOKEN_H
