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

namespace spdlog {
   class logger;
}

class SideshiftPlugin: public Plugin
{
   Q_OBJECT
public:
   SideshiftPlugin(const std::shared_ptr<spdlog::logger>&, QObject *parent);
   ~SideshiftPlugin() override;

   QString name() override { return QLatin1Literal("SideShift.ai"); }
   QString description() override { return tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies"); }
   QString icon() override { return QLatin1Literal("qrc:/images/sideshift_plugin.png"); }
   QString path() override { return QLatin1Literal("qrc:/qml/Plugins/SideShift/SideShiftPopup.qml"); }

   Q_PROPERTY(QStringList inputCurrencies READ inputCurrencies NOTIFY inited)
   QStringList inputCurrencies() const { return inputCurrencies_; }
   Q_PROPERTY(QStringList outputCurrencies READ outputCurrencies NOTIFY inited)
   QStringList outputCurrencies() const { return outputCurrencies_; }
   Q_PROPERTY(QString depositAddress READ depositAddress WRITE setDepositAddr NOTIFY inputCurSelected)
   QString depositAddress() const { return depositAddr_; }
   void setDepositAddr(const QString& addr) { depositAddr_ = addr; }
   Q_PROPERTY(QString conversionRate READ conversionRate NOTIFY inputCurSelected)
   QString conversionRate() const { return convRate_; }
   Q_PROPERTY(QString networkFee READ networkFee NOTIFY inputCurSelected)
   QString networkFee() const { return networkFee_; }
   Q_PROPERTY(QString minAmount READ minAmount NOTIFY inputCurSelected)
   QString minAmount() const { return minAmount_; }
   Q_PROPERTY(QString maxAmount READ maxAmount NOTIFY inputCurSelected)
   QString maxAmount() const { return maxAmount_; }

   Q_PROPERTY(QString orderId READ orderId NOTIFY orderSent)
   QString orderId() const { return orderId_; }
   Q_PROPERTY(QString creationDate READ creationDate NOTIFY orderSent)
   QString creationDate() const { return creationDate_; }

   Q_INVOKABLE void init() override;
   Q_INVOKABLE void inputCurrencySelected(const QString& cur);

signals:
   void inited();
   void inputCurSelected();
   void orderSent();

private:
   void deinit();
   std::string get(const std::string& request);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const std::string    baseURL_{"https://sideshift.ai/api/v2"};
   struct curl_slist*   curlHeaders_{ NULL };
   void* curl_{ nullptr };

   QStringList inputCurrencies_, outputCurrencies_;
   QString     convRate_;
   QString     depositAddr_;
   QString     networkFee_;
   QString     minAmount_, maxAmount_;
   QString     creationDate_;
   QString     orderId_;
};
