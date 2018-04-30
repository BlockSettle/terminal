#ifndef __ASSET__MANAGER_H__
#define __ASSET__MANAGER_H__

#include <memory>
#include <unordered_map>
#include <QObject>
#include <QMutex>
#include "CommonTypes.h"
#include "MetaData.h"


namespace spdlog
{
   class logger;
}

class WalletsManager;
class MarketDataProvider;
class CelerClient;

class AssetManager : public QObject
{
    Q_OBJECT

public:
   AssetManager(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<WalletsManager>& walletsManager
      , const std::shared_ptr<MarketDataProvider>& mdProvider
      , const std::shared_ptr<CelerClient>& celerClient);
   ~AssetManager() = default;

   virtual void init();

public:
   std::vector<std::string> currencies();
   virtual std::vector<std::string> privateShares(bool forceExternal = false);
   virtual double getBalance(const std::string& currency, const std::shared_ptr<bs::Wallet> &wallet = nullptr) const;
   bool checkBalance(const std::string &currency, double amount) const;
   double getPrice(const std::string& currency) const;
   double getTotalAssets();
   double getCashTotal();
   double getCCTotal();
   uint64_t getCCLotSize(const std::string &cc) const;
   bs::Address getCCGenesisAddr(const std::string &cc) const;

   bool hasSecurities() const { return securitiesReceived_; }
   std::vector<QString> securities(bs::network::Asset::Type = bs::network::Asset::Undefined) const;

   bs::network::Asset::Type GetAssetTypeForSecurity(const std::string &security) const;

   bool HaveAssignedAccount() const { return !assignedAccount_.empty(); }
   std::string GetAssignedAccount() const { return assignedAccount_; }

signals:
   void priceChanged(const std::string& currency);
   void balanceChanged(const std::string& currency);
   void fxBalanceLoaded();

   void totalChanged();
   void securitiesReceived();
   void securitiesChanged();

 public slots:
    void onCCSecurityReceived(bs::network::CCSecurityDef);
    void onMDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
    void onMDSecurityReceived(const std::string &security, const bs::network::SecurityDef &sd);
    void onMDSecuritiesReceived();
    void onAccountBalanceLoaded(const std::string& currency, double value);

 private slots:
   void onCelerConnected();
   void onCelerDisconnected();
   void onWalletChanged();

protected:
   bool onAccountBalanceUpdatedEvent(const std::string& data);
   bool securityDef(const std::string &security, bs::network::SecurityDef &) const;

protected:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<WalletsManager>        walletsManager_;
   std::shared_ptr<MarketDataProvider>    mdProvider_;
   std::shared_ptr<CelerClient>           celerClient_;

   bool     securitiesReceived_ = false;
   std::vector<std::string>   currencies_;
   QMutex   mtxCurrencies_;
   std::unordered_map<std::string, double> balances_;
   std::unordered_map<std::string, double> prices_;
   std::unordered_map<std::string, bs::network::SecurityDef>   securities_;
   std::unordered_map<std::string, bs::network::CCSecurityDef> ccSecurities_;

   std::string assignedAccount_;

   double xbtUsdPrice;
};

#endif // __ASSET__MANAGER_H__
