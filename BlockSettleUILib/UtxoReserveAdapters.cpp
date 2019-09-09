#include <spdlog/spdlog.h>
#include "UtxoReserveAdapters.h"

using namespace bs;

OrderUtxoResAdapter::OrderUtxoResAdapter(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent)
   , logger_(logger)
{ }

void bs::OrderUtxoResAdapter::reserved(const std::string &walletId, const std::vector<UTXO> &utxos)
{
   emit reservedUtxosChanged(walletId, utxos);
}

void bs::OrderUtxoResAdapter::unreserved(const std::string &walletId, const std::string &reserveId)
{
   emit reservedUtxosChanged(walletId, {});
}

void bs::OrderUtxoResAdapter::reserve(const bs::core::wallet::TXSignRequest &txReq, const std::string &reserveId)
{
   bs::UtxoReservation::Adapter::reserve(txReq.walletIds.front(), reserveId, txReq.inputs);
}

void OrderUtxoResAdapter::onOrder(const bs::network::Order &order)
{
   if (!parent_ || ((order.status != bs::network::Order::Filled) && (order.status != bs::network::Order::Failed))) {
      return;
   }
   for (const auto &key : reserveKeys(order)) {
      const auto &walletId = parent_->unreserve(key);
      if (walletId.empty()) {
         continue;
      }
   }
}


std::vector<std::string> DealerUtxoResAdapter::reserveKeys(const network::Order &order) const
{
   if (order.settlementId.empty()) {
      return { order.dealerTransaction };
   }
   return { order.settlementId };
}


std::vector<std::string> RequesterUtxoResAdapter::reserveKeys(const network::Order &order) const
{
   return { order.settlementId, order.clOrderId };
}
