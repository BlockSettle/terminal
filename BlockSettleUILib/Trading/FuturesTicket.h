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

QT_BEGIN_NAMESPACE
class QPushButton;
class QLineEdit;
QT_END_NAMESPACE

namespace spdlog {
   class logger;
}
namespace Ui {
    class FuturesTicket;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
   class UTXOReservationManager;
}
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class CCAmountValidator;
class FXAmountValidator;
class QuoteProvider;
class SelectedTransactionInputs;
class SignContainer;
class XbtAmountValidator;


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

public slots:
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
   void onSellSelected();
   void onBuySelected();

private:
   bool eventFilter(QObject *watched, QEvent *evt) override;

   bs::network::Side::Type getSelectedSide() const;
   QString getProduct() const;

   void resetTicket();
   void productSelectionChanged();
   void updateSubmitButton();
   void updateBalances();
   void submitButtonClicked();

   std::unique_ptr<Ui::FuturesTicket> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;

   QFont    invalidBalanceFont_;

   XbtAmountValidator                           *xbtAmountValidator_{};

   QString currentProduct_;
   QString contraProduct_;

   QString currentBidPrice_;
   QString currentOfferPrice_;

   bs::FixedXbtInputs fixedXbtInputs_;

   std::unordered_map<std::string, bs::network::MDInfo>  mdInfo_;
};

#endif // FUTURES_TICKET_H
