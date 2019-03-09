#ifndef __UTXO_RESERVE_ADAPTERS_H__
#define __UTXO_RESERVE_ADAPTERS_H__

#include <memory>
#include <unordered_set>
#include <QObject>
#include "UtxoReservation.h"
#include "CommonTypes.h"
#include "CoreWallet.h"


namespace spdlog {
   class logger;
}

namespace bs {
   class Wallet;

   class OrderUtxoResAdapter : public QObject, public UtxoReservation::Adapter
   {
      Q_OBJECT
   public:
      OrderUtxoResAdapter(const std::shared_ptr<spdlog::logger> &, QObject *parent);
      ~OrderUtxoResAdapter() noexcept override = default;

      void reserve(const bs::core::wallet::TXSignRequest &, const std::string &reserveId);

   signals:
      void reservedUtxosChanged(const std::string &walletId, const std::vector<UTXO> &);

   public slots:
      void onOrder(const bs::network::Order &);

   private:
      virtual std::vector<std::string> reserveKeys(const bs::network::Order &) const = 0;
      void reserved(const std::string &walletId, const std::vector<UTXO> &) override;
      void unreserved(const std::string &walletId, const std::string &reserveId) override;

   private:
      std::shared_ptr<spdlog::logger>  logger_;
   };

   class DealerUtxoResAdapter : public OrderUtxoResAdapter
   {
   public:
      DealerUtxoResAdapter(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
         : OrderUtxoResAdapter(logger, parent) {}
      ~DealerUtxoResAdapter() noexcept override = default;
   private:
      std::vector<std::string> reserveKeys(const bs::network::Order &) const override;
   };

   class RequesterUtxoResAdapter : public OrderUtxoResAdapter
   {
   public:
      RequesterUtxoResAdapter(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
         : OrderUtxoResAdapter(logger, parent) {}
      ~RequesterUtxoResAdapter() noexcept override = default;
   private:
      std::vector<std::string> reserveKeys(const bs::network::Order &) const override;
   };

}  // namespace bs

#endif // UTXO_RESERVE_ADAPTERS_H
