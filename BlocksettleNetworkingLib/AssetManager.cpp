#include <algorithm>
#include <QMutexLocker>
#include <spdlog/spdlog.h>
#include "AssetManager.h"
#include "CelerClient.h"
#include "CelerFindSubledgersForAccountSequence.h"
#include "CelerGetAssignedAccountsListSequence.h"
#include "CommonTypes.h"
#include "CurrencyPair.h"
#include "MarketDataProvider.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include "com/celertech/piggybank/api/subledger/DownstreamSubLedgerProto.pb.h"


AssetManager::AssetManager(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<bs::sync::WalletsManager>& walletsManager
      , const std::shared_ptr<MarketDataProvider>& mdProvider
      , const std::shared_ptr<BaseCelerClient>& celerClient)
 : logger_(logger)
 , walletsManager_(walletsManager)
 , mdProvider_(mdProvider)
 , celerClient_(celerClient)
{
   connect(this, &AssetManager::ccPriceChanged, [this] { emit totalChanged(); });
   connect(this, &AssetManager::xbtPriceChanged, [this] { emit totalChanged(); });
   connect(this, &AssetManager::balanceChanged, [this] { emit totalChanged(); });
}

void AssetManager::init()
{
   connect(mdProvider_.get(), &MarketDataProvider::MDSecurityReceived, this, &AssetManager::onMDSecurityReceived);
   connect(mdProvider_.get(), &MarketDataProvider::MDSecuritiesReceived, this, &AssetManager::onMDSecuritiesReceived);

   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &AssetManager::onWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsReady, this, &AssetManager::onWalletChanged);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::blockchainEvent, this, &AssetManager::onWalletChanged);

   connect(celerClient_.get(), &BaseCelerClient::OnConnectedToServer, this, &AssetManager::onCelerConnected);
   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed, this, &AssetManager::onCelerDisconnected);

   celerClient_->RegisterHandler(CelerAPI::SubLedgerSnapshotDownstreamEventType, [this](const std::string& data) { return onAccountBalanceUpdatedEvent(data); });
}

double AssetManager::getBalance(const std::string& currency, const std::shared_ptr<bs::sync::Wallet> &wallet) const
{
   if (currency == bs::network::XbtCurrency) {
      if (wallet == nullptr) {
         return walletsManager_->getSpendableBalance();
      }
      return wallet->getSpendableBalance();
   }

   const auto itCC = ccSecurities_.find(currency);
   if (itCC != ccSecurities_.end()) {
      const auto &priWallet = walletsManager_->getPrimaryWallet();
      if (priWallet) {
         const auto &group = priWallet->getGroup(bs::hd::BlockSettle_CC);
         if (group) {
            const auto &wallet = group->getLeaf(currency);
            if (wallet) {
               return wallet->getTotalBalance();
            }
         }
      }
      return 1.0 / itCC->second.nbSatoshis;
   }

   auto it = balances_.find(currency);
   if (it != balances_.end()) {
      return it->second;
   }

   return 0.0;
}

double AssetManager::getPrice(const std::string& currency) const
{
   auto it = prices_.find(currency);
   if (it != prices_.end()) {
      return it->second;
   }

   return 0.0;
}

bool AssetManager::checkBalance(const std::string &currency, double amount) const
{
   if (currency.empty()) {
      return false;
   }
   const auto balance = getBalance(currency);
   return ((amount <= balance) || qFuzzyCompare(amount, balance));
}


std::vector<std::string> AssetManager::currencies()
{
   if (balances_.size() != currencies_.size()) {
      QMutexLocker lock(&mtxCurrencies_);
      currencies_.clear();

      for (const auto balance : balances_) {
         currencies_.push_back(balance.first);
      }

      std::sort(currencies_.begin(), currencies_.end());
   }

   return currencies_;
}

std::vector<std::string> AssetManager::privateShares(bool forceExternal)
{
   std::vector<std::string> result;

   const auto &priWallet = walletsManager_->getPrimaryWallet();
   if (!forceExternal && priWallet) {
      const auto &group = priWallet->getGroup(bs::hd::BlockSettle_CC);
      if (group) {
         const auto &leaves = group->getAllLeaves();
         for (const auto &leaf : leaves) {
            if (leaf->getSpendableBalance() > 0) {
               result.push_back(leaf->shortName());
            }
         }
      }
   }
   else {
      for (const auto &cc : ccSecurities_) {
         result.push_back(cc.first);
      }
   }
   return result;
}

std::vector<QString> AssetManager::securities(bs::network::Asset::Type assetType) const
{
   std::vector<QString> rv;
   for (auto security : securities_) {
      if ((security.second.assetType == assetType) || (assetType == bs::network::Asset::Undefined)) {
         rv.push_back(QString::fromStdString(security.first));
      }
   }
   return rv;
}

bool AssetManager::securityDef(const std::string &security, bs::network::SecurityDef &sd) const
{
   const auto itSec = securities_.find(security);
   if (itSec == securities_.end())
      return false;
   sd = itSec->second;
   return true;
}

bs::network::Asset::Type AssetManager::GetAssetTypeForSecurity(const std::string &security) const
{
   bs::network::Asset::Type assetType = bs::network::Asset::Type::Undefined;
   bs::network::SecurityDef sd;
   if (securityDef(security, sd)) {
      assetType = sd.assetType;
   }

   return assetType;
}

void AssetManager::onWalletChanged()
{
   emit balanceChanged(bs::network::XbtCurrency);
}

void AssetManager::onMDSecurityReceived(const std::string &security, const bs::network::SecurityDef &sd)
{
   if (sd.assetType != bs::network::Asset::PrivateMarket) {
      securities_[security] = sd;
   }
}

void AssetManager::onMDSecuritiesReceived()
{
   securitiesReceived_ = true;
}

void AssetManager::onCCSecurityReceived(bs::network::CCSecurityDef ccSD)
{
   ccSecurities_[ccSD.product] = ccSD;

   bs::network::SecurityDef sd = { bs::network::Asset::PrivateMarket};
   securities_[ccSD.securityId] = sd;
}

void AssetManager::onMDUpdate(bs::network::Asset::Type at, const QString &security, bs::network::MDFields fields)
{
   if ((at == bs::network::Asset::Undefined) || security.isEmpty()) {
      return;
   }
   double lastPx = 0;
   double bidPrice = 0;

   double productPrice = 0;
   CurrencyPair cp(security.toStdString());
   std::string ccy;

   switch (at) {
   case bs::network::Asset::PrivateMarket:
      ccy = cp.NumCurrency();
      break;
   case bs::network::Asset::SpotXBT:
      ccy = cp.DenomCurrency();
      break;
   default:
      return;
   }

   if (ccy.empty()) {
      return;
   }

   for (const auto &field : fields) {
      if (field.type == bs::network::MDField::PriceLast) {
         lastPx = field.value;
         break;
      } else  if (field.type == bs::network::MDField::PriceBid) {
         bidPrice = field.value;
      }
   }

   productPrice = (lastPx > 0) ? lastPx : bidPrice;

   if (productPrice > 0) {
      if (ccy == cp.DenomCurrency()) {
         productPrice = 1 / productPrice;
      }
      prices_[ccy] = productPrice;
      if (at == bs::network::Asset::PrivateMarket) {
         emit ccPriceChanged(ccy);
      } else {
         sendUpdatesOnXBTPrice(ccy);
      }
   }
}


double AssetManager::getCashTotal()
{
   double total = 0;

   for (const auto &currency : currencies()) {
      total += getBalance(currency) * getPrice(currency);
   }
   return total;
}

double AssetManager::getCCTotal()
{
   double total = 0;

   for (const auto &ccSec : ccSecurities_) {
      total += getBalance(ccSec.first) * getPrice(ccSec.first);
   }
   return total;
}

double AssetManager::getTotalAssets()
{
   return walletsManager_->getTotalBalance() + getCashTotal() + getCCTotal();
}

uint64_t AssetManager::getCCLotSize(const std::string &cc) const
{
   const auto ccIt = ccSecurities_.find(cc);
   if (ccIt == ccSecurities_.end()) {
      return 0;
   }
   return ccIt->second.nbSatoshis;
}

bs::Address AssetManager::getCCGenesisAddr(const std::string &cc) const
{
   const auto ccIt = ccSecurities_.find(cc);
   if (ccIt == ccSecurities_.end()) {
      return {};
   }
   return ccIt->second.genesisAddr;
}

void AssetManager::onCelerConnected()
{
   auto cb = [this](const std::vector<std::string>& accounts) {
      if (accounts.size() == 1) {
         assignedAccount_ = accounts[0];
         logger_->debug("[AssetManager] assigned account: {}", assignedAccount_);

         auto onLoaded = [this](const std::vector<CelerFindSubledgersForAccountSequence::currencyBalancePair>& currencyBalancePairs)
         {
            for (const auto& cbp : currencyBalancePairs) {
               this->onAccountBalanceLoaded(cbp.first, cbp.second);
            }

            emit fxBalanceLoaded();
         };

         auto subledgerSeq = std::make_shared<CelerFindSubledgersForAccountSequence>(this->logger_, accounts[0], onLoaded);

         this->celerClient_->ExecuteSequence(subledgerSeq);
      } else {
         this->logger_->error("[AssetManager::onCelerConnected] too many accounts ({})", accounts.size());
         for (const auto &account : accounts) {
            this->logger_->error("[AssetManager::onCelerConnected] acc: {}", account);
         }
      }
   };

   auto seq = std::make_shared<CelerGetAssignedAccountsListSequence>(logger_, cb);
   celerClient_->ExecuteSequence(seq);
}

void AssetManager::onCelerDisconnected()
{
   std::vector<std::string> securitiesToClear;
   for (const auto &security : securities_) {
      if (security.second.assetType != bs::network::Asset::PrivateMarket) {
         securitiesToClear.push_back(security.first);
      }
   }
   for (const auto &security : securitiesToClear) {
      securities_.erase(security);
   }

   balances_.clear();
   currencies_.clear();
   emit securitiesChanged();
   emit fxBalanceCleared();
   emit totalChanged();
}

bool AssetManager::onAccountBalanceUpdatedEvent(const std::string &data)
{
   com::celertech::piggybank::api::subledger::SubLedgerSnapshotDownstreamEvent snapshot;
   if (!snapshot.ParseFromString(data)) {
      logger_->error("[AssetManager::onAccountBalanceUpdatedEvent] faied to parse SubLedgerSnapshotDownstreamEvent");
      return false;
   }

   logger_->debug("[AssetManager::onAccountBalanceUpdatedEvent] get update:\n{}", snapshot.DebugString());
   onAccountBalanceLoaded(snapshot.currency(), snapshot.netposition());
   return true;
}

void AssetManager::onAccountBalanceLoaded(const std::string& currency, double value)
{
   if (currency == bs::network::XbtCurrency) {
      return;
   }
   balances_[currency] = value;
   emit balanceChanged(currency);
}

void AssetManager::sendUpdatesOnXBTPrice(const std::string& ccy)
{
   auto currentTime = QDateTime::currentDateTimeUtc();
   bool emitUpdate = false;

   auto it = xbtPriceUpdateTimes_.find(ccy);

   if (it == xbtPriceUpdateTimes_.end()) {
      emitUpdate = true;
      xbtPriceUpdateTimes_.emplace(ccy, currentTime);
   } else {
      if (it->second.secsTo(currentTime) >= 30) {
         it->second = currentTime;
         emitUpdate = true;
      }
   }

   if (emitUpdate) {
      emit xbtPriceChanged(ccy);
   }
}
