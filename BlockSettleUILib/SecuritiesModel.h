/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SECURITIES_MODEL_H__
#define __SECURITIES_MODEL_H__

#include <QAbstractItemModel>

#include <memory>

class AssetManager;
class BaseSecurityNode;
class RootSecuritiesNode;

class SecuritiesModel : public QAbstractItemModel
{
Q_OBJECT

public:
   SecuritiesModel(const std::shared_ptr<AssetManager> &assetMgr
      , const QStringList &showSettings, QObject *parent = nullptr);
   ~SecuritiesModel() noexcept override = default;

   SecuritiesModel(const SecuritiesModel&) = delete;
   SecuritiesModel& operator = (const SecuritiesModel&) = delete;
   SecuritiesModel(SecuritiesModel&&) = delete;
   SecuritiesModel& operator = (SecuritiesModel&&) = delete;

   QStringList getVisibilitySettings() const;

   int columnCount(const QModelIndex & parent = QModelIndex()) const override;
   int rowCount(const QModelIndex & parent = QModelIndex()) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

   Qt::ItemFlags flags(const QModelIndex & index) const override;

   QVariant data(const QModelIndex& index, int role) const override;
   bool setData(const QModelIndex & index, const QVariant & value, int role) override;

   QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const override;

   QModelIndex parent(const QModelIndex& child) const override;
   bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

private:
   BaseSecurityNode* getNodeByIndex(const QModelIndex& index) const;

private:
   std::shared_ptr<RootSecuritiesNode> rootNode_;
};

#endif // __SECURITIES_MODEL_H__