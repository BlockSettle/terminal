/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CC_PORTFOLIO_MODEL__
#define __CC_PORTFOLIO_MODEL__

#include <memory>
#include <QAbstractItemModel>
#include "Wallets/SignerDefs.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AssetGroupNode;
class AssetManager;
class AssetNode;
class RootAssetGroupNode;

class CCPortfolioModel : public QAbstractItemModel
{
public:
   [[deprecated]] CCPortfolioModel(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<AssetManager>& assetManager
      , QObject *parent = nullptr);
   CCPortfolioModel(QObject* parent = nullptr);
   ~CCPortfolioModel() noexcept override = default;

   CCPortfolioModel(const CCPortfolioModel&) = delete;
   CCPortfolioModel& operator = (const CCPortfolioModel&) = delete;

   CCPortfolioModel(CCPortfolioModel&&) = delete;
   CCPortfolioModel& operator = (CCPortfolioModel&&) = delete;

   void onHDWallet(const bs::sync::WalletInfo&);
   void onHDWalletDetails(const bs::sync::HDWalletData&);
   void onWalletBalance(const bs::sync::WalletBalanceData&);
   void onBalance(const std::string& currency, double balance);

private:
   enum PortfolioColumns
   {
      AssetNameColumn,
      BalanceColumn,
      XBTValueColumn,
      PortfolioColumnsCount
   };

   AssetNode* getNodeByIndex(const QModelIndex& index) const;

public:
   int columnCount(const QModelIndex & parent = QModelIndex()) const override;
   int rowCount(const QModelIndex & parent = QModelIndex()) const override;

   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
   QVariant data(const QModelIndex& index, int role) const override;

   QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const override;

   QModelIndex parent(const QModelIndex& child) const override;
   bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

private slots:
   void onFXBalanceLoaded();  // deprecated
   void onFXBalanceCleared(); // deprecated

   void onFXBalanceChanged(const std::string& currency); // deprecated

   void onXBTPriceChanged(const std::string& currency);  //deprecated
   void onCCPriceChanged(const std::string& currency);   // deprecated

   void reloadXBTWalletsList();  // deprecated
   void updateXBTBalance();   // deprecated

   void reloadCCWallets(); // deprecated
   void updateCCBalance(); // deprecated

private:
   void updateWalletBalance(const std::string& walletId);

private:
   [[deprecated]] std::shared_ptr<AssetManager> assetManager_;
   [[deprecated]] std::shared_ptr<bs::sync::WalletsManager> walletsManager_;

   std::shared_ptr<RootAssetGroupNode> root_ = nullptr;
   std::map<std::string, std::string>  leafIdToRootId_;
   std::map<std::string, double>       leafBalances_;
};

#endif // __CC_PORTFOLIO_MODEL__