#ifndef __REQ_XBT_SETTLEMENT_CONTAINER_H__
#define __REQ_XBT_SETTLEMENT_CONTAINER_H__

#include <memory>
#include <unordered_set>
#include "AddressVerificator.h"
#include "SettlementContainer.h"
#include "SettlementWallet.h"
#include "TransactionData.h"

namespace spdlog {
   class logger;
}
class ArmoryConnection;
class SignContainer;
class QuoteProvider;
class TransactionData;
class WalletsManager;


class ReqXBTSettlementContainer : public bs::SettlementContainer
{
   Q_OBJECT
public:
   ReqXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &, const bs::network::Order &
      , const std::shared_ptr<WalletsManager> &, const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<TransactionData> &, const std::unordered_set<std::string> &bsAddresses
      , const std::shared_ptr<SignContainer> &, const std::shared_ptr<ArmoryConnection> &, bool autoSign);
   ~ReqXBTSettlementContainer() override = default;

   bool accept(const SecureBinaryData &password = {}) override;
   bool cancel() override;

   bool isAcceptable() const override;

   void activate() override;
   void deactivate() override;

/*   std::string id() const override { return order_.settlementId; }
   bs::network::Asset::Type assetType() const override { return order_.assetType; }
   std::string security() const override { return order_.security; }
   std::string product() const override { return order_.product; }
   bs::network::Side::Type side() const override { return order_.side; }
   double quantity() const override { return order_.quantity; }
   double price() const override { return order_.price; }
   double amount() const override { return amount_; }

   bool weSell() const { return weSell_; }
   uint64_t fee() const { return fee_; }*/

private:
};

#endif // __REQ_XBT_SETTLEMENT_CONTAINER_H__
