/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ORDERLISTMODEL_H
#define ORDERLISTMODEL_H

#include <QAbstractItemModel>
#include <QColor>
#include <QFont>
#include <QVariant>

#include "CommonTypes.h"
#include <memory>
#include <vector>
#include <deque>

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
         class Response_UpdateOrder;
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

   OrderListModel(QObject* parent = nullptr);
   ~OrderListModel() noexcept override = default;

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex &index) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant headerData(int section, Qt::Orientation orientation,
                       int role = Qt::DisplayRole) const override;

   //void onOrdersUpdate(const std::vector<bs::network::Order> &);

public slots:
   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);
   void onDisconnected();

signals:
   void newOrder(const QPersistentModelIndex &index);
   void selectRow(const QPersistentModelIndex &row);

private:
   enum class DataType {
      Data = 0,
      Group
   };

   struct IndexHelper {
      IndexHelper *parent_;
      void *data_;
      DataType type_;

      IndexHelper(IndexHelper *parent, void *data, DataType type)
         : parent_(parent)
         , data_(data)
         , type_(type)
      {}
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

   struct Group
   {
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

#endif // ORDERLISTMODEL_H
