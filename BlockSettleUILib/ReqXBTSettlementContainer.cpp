#include "ReqXBTSettlementContainer.h"
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "SignContainer.h"
#include "QuoteProvider.h"
#include "TransactionData.h"
#include "WalletsManager.h"


ReqXBTSettlementContainer::ReqXBTSettlementContainer(const std::shared_ptr<spdlog::logger> &logger, const bs::network::Order &order
   , const std::shared_ptr<WalletsManager> &walletsMgr, const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<TransactionData> &txData, const std::unordered_set<std::string> &bsAddresses
   , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<ArmoryConnection> &armory, bool autoSign)
   : bs::SettlementContainer(armory)
{
}

bool ReqXBTSettlementContainer::accept(const SecureBinaryData &password)
{
   stopTimer();
   return true;
}

bool ReqXBTSettlementContainer::cancel()
{
   stopTimer();
   return true;
}

bool ReqXBTSettlementContainer::isAcceptable() const
{
   return true;
}

void ReqXBTSettlementContainer::activate()
{
}

void ReqXBTSettlementContainer::deactivate()
{
}
