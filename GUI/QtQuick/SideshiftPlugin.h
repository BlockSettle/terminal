/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include "Plugin.h"
#include <string>
#include <QQmlApplicationEngine>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include "Message/Worker.h"

namespace spdlog {
   class logger;
}
class CoinImageProvider;

struct Currency
{
   QString name;
   QString coin;
   QString network;
};

class CurrencyListModel : public QAbstractListModel
{
   Q_OBJECT
public:
   enum CurrencyRoles {
      NameRole = Qt::UserRole + 1,
      CoinRole, 
      NetworkRole
   };

   CurrencyListModel(QObject* parent = nullptr);
   
   int rowCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   void reset(const QList<Currency>& currencies);

private:
   QList<Currency> currencies_;
};

class CurrencyFilterModel : public QSortFilterProxyModel
{
   Q_OBJECT
   Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)

public:
   CurrencyFilterModel(QObject* parent = nullptr);

   const QString& filter() const;
   void setFilter(const QString& filter);

protected:
   bool filterAcceptsRow(int source_row,
      const QModelIndex& source_parent) const override;

signals:
   void changed();

private:
   QString filter_;
};

class SideshiftPlugin: public Plugin, protected bs::WorkerPool
{
   class GetHandler;
   class PostHandler;

   friend class CoinImageProvider;
   friend class GetHandler;
   friend class PostHandler;
   Q_OBJECT
public:
   SideshiftPlugin(const std::shared_ptr<spdlog::logger>&, QQmlApplicationEngine &
      , QObject *parent);
   ~SideshiftPlugin() override;

   QString name() const override { return QLatin1Literal("SideShift.ai"); }
   QString description() const override { return tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies"); }
   QString icon() const override { return QLatin1Literal("qrc:/images/sideshift_plugin.png"); }
   QString path() const override { return QLatin1Literal("qrc:/qml/Plugins/SideShift/SideShiftPopup.qml"); }

   Q_PROPERTY(QAbstractItemModel* inputCurrenciesModel READ inputCurrenciesModel NOTIFY inited)
   QAbstractItemModel* inputCurrenciesModel() { return inputFilterModel_; } 
   Q_PROPERTY(QAbstractItemModel* outputCurrenciesModel READ outputCurrenciesModel NOTIFY inited)
   QAbstractItemModel* outputCurrenciesModel() { return outputFilterModel_; } 

   Q_PROPERTY(QString conversionRate READ conversionRate NOTIFY pairUpdated)
   QString conversionRate() const { return convRate_; }
   Q_PROPERTY(QString minAmount READ minAmount NOTIFY pairUpdated)
   QString minAmount() const { return minAmount_; }
   Q_PROPERTY(QString maxAmount READ maxAmount NOTIFY pairUpdated)
   QString maxAmount() const { return maxAmount_; }
   Q_PROPERTY(QString inputNetwork READ inputNetwork WRITE setInputNetwork NOTIFY pairUpdated)
   QString inputNetwork() const { return inputNetwork_; }
   void setInputNetwork(const QString&);

   Q_PROPERTY(QString networkFee READ networkFee NOTIFY orderSent)
   QString networkFee() const { return networkFee_; }
   Q_PROPERTY(QString depositAddress READ depositAddress NOTIFY orderSent)
   QString depositAddress() const { return depositAddr_; }
   Q_PROPERTY(QString orderId READ orderId NOTIFY orderSent)
   QString orderId() const { return orderId_; }
   Q_PROPERTY(QString creationDate READ creationDate NOTIFY orderSent)
   QString creationDate() const { return creationDate_; }
   Q_PROPERTY(QString status READ status NOTIFY orderSent)
   QString status() const { return shiftStatus_; }

   Q_INVOKABLE void init() override;
   Q_INVOKABLE void inputCurrencySelected(const QString& cur);
   Q_INVOKABLE bool sendShift(const QString& recvAddr);
   Q_INVOKABLE void updateShiftStatus();

signals:
   void inited();
   void inputCurSelected();
   void inputSelected();
   void orderSent();
   void pairUpdated();

protected:
   std::shared_ptr<bs::Worker> worker(const std::shared_ptr<bs::InData>&) override final;

private:
   void deinit();
   std::string get(const std::string& request);
   QString statusToQString(const std::string& status) const;
   void getPair();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const std::string    baseURL_{"https://sideshift.ai/api/v2"};
   const std::string    affiliateId_{"a9KgPNzTn"};
   struct curl_slist*   curlHeaders_{ NULL };
   void* curl_{ nullptr };
   std::mutex  curlMtx_;

   QString     inputNetwork_;
   QString     inputCurrency_;
   QString     convRate_;
   QString     depositAddr_;
   QString     networkFee_;
   QString     minAmount_, maxAmount_;
   QString     creationDate_, expireDate_;
   QString     orderId_;
   QString     shiftStatus_;

   CurrencyListModel* inputListModel_;
   CurrencyFilterModel* inputFilterModel_;
   CurrencyListModel* outputListModel_;
   CurrencyFilterModel* outputFilterModel_;
};
