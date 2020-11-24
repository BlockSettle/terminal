/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MockAssetMgr.h"
#include "CurrencyPair.h"
#include "Wallets/SyncWallet.h"


void MockAssetManager::init()
{
   prices_["USD"] = 1.0 / 4000;
   prices_["EUR"] = 1.0 / 3400;
   prices_["GBP"] = 1.0 / 3000;
   prices_["SEK"] = 1.0 / 36000;
   prices_["BLK"] = 1.0 / 0.023;

   balances_["XBT"] = 12.345;
   balances_["USD"] = 12345.67;
   balances_["EUR"] = 2345.67;
   balances_["GBP"] = 3456.78;
   balances_["SEK"] = 45678.9;
   balances_["BLK"] = 9123;

   for (const auto &spotFX : {"EUR/GBP", "EUR/SEK", "EUR/USD", "GBP/SEK", "GPB/USD", "USD/SEK"}) {
      securities_[spotFX] = bs::network::SecurityDef {
         bs::network::Asset::SpotFX};
   }
   for (const auto &spotXBT : { "XBT/USD", "XBT/GBP", "XBT/EUR", "XBT/SEK" }) {
      securities_[spotXBT] = bs::network::SecurityDef {
         bs::network::Asset::SpotXBT};
   }
   for (const auto &cc : { "BLK/XBT" }) {
      securities_[cc] = bs::network::SecurityDef{
         bs::network::Asset::PrivateMarket};

      const CurrencyPair cp(cc);
      ccSecurities_[cp.NumCurrency()] = bs::network::CCSecurityDef{ cc, cp.NumCurrency(),  {}, 526 };
   }
   securitiesReceived_ = true;
}

double MockAssetManager::getBalance(const std::string& currency, bool includeZc, const std::shared_ptr<bs::sync::Wallet> &) const
{
   auto it = balances_.find(currency);
   if (it != balances_.end()) {
      return it->second;
   }
   return 0.0;
}

std::vector<std::string> MockAssetManager::privateShares(bool forceExt)
{
   return { "BLK" };
}
