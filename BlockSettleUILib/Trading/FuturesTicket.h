/*

***********************************************************************************
* Copyright (C) 2020 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef FUTURES_TICKET_H
#define FUTURES_TICKET_H

#include <QFont>
#include <QWidget>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "BSErrorCode.h"
#include "CommonTypes.h"
#include "UtxoReservationToken.h"
#include "XBTAmount.h"
#include "UiUtils.h"

namespace spdlog {
   class logger;
}
namespace Ui {
    class FuturesTicket;
}
class AssetManager;
class QuoteProvider;
class XbtAmountValidator;
class QLabel;


class FuturesTicket : public QWidget
{
Q_OBJECT

public:
   FuturesTicket(QWidget* parent = nullptr);
   ~FuturesTicket() override;

   void init(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<AssetManager> &assetManager
      , const std::shared_ptr<QuoteProvider> &quoteProvider);

signals:
   void sendFutureRequestToPB(const bs::network::FutureRequest &details);

public slots:
   void setType(bs::network::Asset::Type type);
   void SetProductAndSide(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice, bs::network::Side::Type side);
   void setSecurityId(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);
   void setSecurityBuy(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);
   void setSecuritySell(const QString& productGroup, const QString& currencyPair
      , const QString& bidPrice, const QString& offerPrice);
   void SetCurrencyPair(const QString& currencyPair);

   void onMDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);

private slots:
   void onAmountEdited(const QString &);
   void updatePanel();
   void onCloseAll();

private:
   bool eventFilter(QObject *watched, QEvent *evt) override;

   std::string getProduct() const;
   double getQuantity() const;

   void resetTicket();
   void productSelectionChanged();
   void submit(bs::network::Side::Type side);
   void sendRequest(bs::network::Side::Type side, bs::XBTAmount amount);

   std::unique_ptr<Ui::FuturesTicket> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;

   XbtAmountValidator *xbtAmountValidator_{};

   bs::network::Asset::Type type_{};
   std::string currentProduct_;
   QString security_;

   std::map<bs::network::Asset::Type, std::unordered_map<std::string, std::map<bs::XBTAmount, bs::network::MDInfo>>> mdInfo_;

   std::vector<std::array<QLabel*, 2>> labels_;
};

#endif // FUTURES_TICKET_H