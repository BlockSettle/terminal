/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CC_PORTFOLIO_MODEL__
#define __CC_PORTFOLIO_MODEL__

#include <memory>
#include <QAbstractItemModel>

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
   CCPortfolioModel(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<AssetManager>& assetManager
      , QObject *parent = nullptr);
   ~CCPortfolioModel() noexcept override = default;

   CCPortfolioModel(const CCPortfolioModel&) = delete;
   CCPortfolioModel& operator = (const CCPortfolioModel&) = delete;

   CCPortfolioModel(CCPortfolioModel&&) = delete;
   CCPortfolioModel& operator = (CCPortfolioModel&&) = delete;

   std::shared_ptr<AssetManager> assetManager();
   std::shared_ptr<bs::sync::WalletsManager> walletsManager();

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
   void onFXBalanceLoaded();
   void onFXBalanceCleared();

   void onFXBalanceChanged(const std::string& currency);

   void onXBTPriceChanged(const std::string& currency);
   void onCCPriceChanged(const std::string& currency);

   void reloadXBTWalletsList();
   void updateXBTBalance();

   void reloadCCWallets();
   void updateCCBalance();

private:
   std::shared_ptr<AssetManager>             assetManager_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;

   std::shared_ptr<RootAssetGroupNode> root_ = nullptr;
};

#endif // __CC_PORTFOLIO_MODEL__