/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ORDERLISTMODEL_H
#define ORDERLISTMODEL_H

#include <QAbstractItemModel>
#include <QFont>
#include <QColor>

#include "CommonTypes.h"

#include <memory>
#include <vector>
#include <deque>

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
         class Response_DeliveryObligationsRequest;
         class Response_UpdateOrdersAndObligations;
      }
   }
}

class AssetManager;

class OrderListModel : public QAbstractItemModel
{
    Q_OBJECT

public:
   struct Header {
      enum Index {
         Time = 0,
         NameColumn = 0,
         Product,
         Side,
         Quantity,
         Price,
         Value,
         Status,
         last
      };
      static QString toString(Index);
   };

   struct DeliveryObligationData
   {
      std::string       bsAddress;
      bs::XBTAmount     deliveryAmount;

      bool isValid() const {
         return !bsAddress.empty();
      }
   };

   OrderListModel(const std::shared_ptr<AssetManager> &, QObject *parent = nullptr);
   ~OrderListModel() noexcept override = default;

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex &index) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant headerData(int section, Qt::Orientation orientation,
                       int role = Qt::DisplayRole) const override;

   bool DeliveryRequired(const QModelIndex &index);

   DeliveryObligationData getDeliveryObligationData(const QModelIndex &index) const;
public slots:
   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);
   void onDisconnected();

signals:
   void newOrder(const QPersistentModelIndex &index);
   void selectRow(const QPersistentModelIndex &row);

private:
   enum class DataType {
      Data = 0,
      Group,
      Market,
      StatusGroup
   };

   struct IndexHelper {
      IndexHelper *parent_;
      void *data_;
      DataType type_;

      IndexHelper(IndexHelper *parent, void *data, DataType type)
         : parent_(parent)
         , data_(data)
         , type_(type)
      {
      }
   };

   struct Data {
      QString time_;
      QString product_;
      QString side_;
      QString quantity_;
      QString price_;
      QString value_;
      QString status_;
      QString id_;
      QColor statusColor_;
      IndexHelper idx_;

      Data(const QString &time, const QString &prod,
         const QString &side, const QString &quantity, const QString &price,
         const QString &value, const QString &status, const QString &id,
         IndexHelper *parent)
         : time_(time)
         , product_(prod)
         , side_(side)
         , quantity_(quantity)
         , price_(price)
         , value_(value)
         , status_(status)
         , id_(id)
         , statusColor_(Qt::white)
         , idx_(parent, this, DataType::Data)
      {
      }
   };

   struct Group {
      std::deque<std::unique_ptr<Data>>   rows_;
      QString                             security_;
      IndexHelper                         idx_;

      Group(const QString &sec, IndexHelper *parent)
         : security_(sec)
         , idx_(parent, this, DataType::Group)
      {
      }

      virtual ~Group() = default;

      virtual void addOrder(const bs::network::Order& order);

      virtual QVariant getQuantity() const;
      virtual QVariant getQuantityColor() const;

      virtual QVariant getValue() const;
      virtual QVariant getValueColor() const;
      virtual QVariant getPrice() const;

      virtual QVariant getStatus() const;
   protected:
      void addRow(const bs::network::Order& order);
   };

   struct FuturesGroup : public Group
   {
      FuturesGroup(const QString &sec, IndexHelper *parent)
         : Group(sec, parent)
      {}

      ~FuturesGroup() override = default;

      void addOrder(const bs::network::Order& order) override;

      QVariant getQuantity() const override;
      QVariant getQuantityColor() const override;

      QVariant getValue() const override;
      QVariant getValueColor() const override;

   private:
      double quantity_ = 0;
      double value_ = 0;
   };

   struct FuturesDeliveryGroup : public Group
   {
      FuturesDeliveryGroup(const QString &sec, IndexHelper *parent
                           , int64_t quantity, double price
                           , const std::string& bsAddress
                           , const bool delivered);

      ~FuturesDeliveryGroup() override = default;

      QVariant getQuantity() const override;
      QVariant getQuantityColor() const override;

      QVariant getValue() const override;
      QVariant getValueColor() const override;

      QVariant getPrice() const override;
      QVariant getStatus() const override;

      bool deliveryRequired() const;

      DeliveryObligationData getDeliveryObligationData() const;

   private:
      double quantity_ = 0;
      double value_ = 0;
      double price_ = 0;

      bs::XBTAmount  toDeliver_;
      std::string    bsAddress_;

      bool           delivered_;
   };


   struct Market {
      std::vector<std::unique_ptr<Group>> rows_;

      bs::network::Asset::Type            assetType_;
      QString                             name_;
      IndexHelper                         idx_;
      QFont                               font_;

      Market(const bs::network::Asset::Type assetType, IndexHelper *parent)
         : assetType_{assetType}
         , name_{tr(bs::network::Asset::toString(assetType))}
         , idx_(parent, this, DataType::Market)
      {
         font_.setBold(true);
      }
   };

   struct StatusGroup {
      std::vector<std::unique_ptr<Market>> rows_;
      QString name_;
      IndexHelper idx_;
      int row_;

      enum Type {
         first = 0,
         PendingSettlements = first,
         UnSettled,
         Settled,
         Undefined
      };

      StatusGroup(const QString &name, int row)
         : name_(name)
         , idx_(nullptr, this, DataType::StatusGroup)
         , row_(row)
      {
      }

      static QString toString(Type);
   };

   static StatusGroup::Type getStatusGroup(const bs::network::Order &);

   void onOrderUpdated(const bs::network::Order &);
   int findGroup(Market *market, Group *group) const;
   int findMarket(StatusGroup *statusGroup, Market *market) const;
   std::pair<Group*, int> findItem(const bs::network::Order &order);
   void setOrderStatus(Group *group, int index, const bs::network::Order& order,
      bool emitUpdate = false);
   void removeRowIfContainerChanged(const bs::network::Order &order, int &oldOrderRow);
   void findMarketAndGroup(const bs::network::Order &order, Market *&market, Group *&group);
   void createGroupsIfNeeded(const bs::network::Order &order, Market *&market, Group *&group);

   void reset();
   void processUpdateOrders(const Blocksettle::Communication::ProxyTerminalPb::Response_UpdateOrdersAndObligations &msg);
   void resetLatestChangedStatus(const Blocksettle::Communication::ProxyTerminalPb::Response_UpdateOrdersAndObligations &message);

   void DisplayFuturesDeliveryRow(const Blocksettle::Communication::ProxyTerminalPb::Response_DeliveryObligationsRequest &obligation);

   bool isFutureDeliveryIndex(const QModelIndex &index) const;
   FuturesDeliveryGroup* GetFuturesDeliveryGroup(const QModelIndex &index) const;

private:
   std::shared_ptr<AssetManager>                      assetManager_;
   std::unordered_map<std::string, StatusGroup::Type> groups_;
   std::unique_ptr<StatusGroup>                       unsettled_;
   std::unique_ptr<StatusGroup>                       settled_;
   std::unique_ptr<StatusGroup>                       pendingFuturesSettlement_;
   QDateTime latestOrderTimestamp_;

   std::vector<std::pair<int64_t, int>> sortedPeviousOrderStatuses_{};
   QDateTime latestChangedTimestamp_;

   std::set<std::string> knownIds_;

   bool connected_{};
};

#endif // ORDERLISTMODEL_H
