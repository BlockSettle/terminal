#ifndef ORDERLISTMODEL_H
#define ORDERLISTMODEL_H

#include <QStandardItemModel>
#include "CommonTypes.h"
#include <memory>

class QuoteProvider;
class AssetManager;

class OrderListModel : public QStandardItemModel
{
    Q_OBJECT

public:
   OrderListModel(std::shared_ptr<QuoteProvider>, const std::shared_ptr<AssetManager> &, QObject* parent);
   ~OrderListModel() = default;

public slots:
   void onOrderUpdated(const bs::network::Order &);

public:
   struct Header {
      enum Index {
         first,
         Time = first,
         SecurityID,
         Product,
         Side,
         Quantity,
         Price,
         Value,
         Status,
         last
      };
      static QString toString(Index);
      static QStringList labels();
   };

private:
   struct StatusGroup {
      enum Type {
         first,
         UnSettled = first,
         Settled,
         last
      };
      static QString toString(Type);
   };

   std::pair<QStandardItem *, int> findItem(const bs::network::Order &);
   static void setOrderStatus(QStandardItem *, const bs::network::Order &);
   static StatusGroup::Type getStatusGroup(const bs::network::Order &);

private:
   std::shared_ptr<QuoteProvider>   quoteProvider_;
   std::shared_ptr<AssetManager>    assetManager_;
   std::unordered_map<std::string, StatusGroup::Type> groups_;
};

#endif // ORDERLISTMODEL_H
