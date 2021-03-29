/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __PORFOLIO_WIDGET_H__
#define __PORFOLIO_WIDGET_H__

#include <QWidget>
#include <QMenu>
#include <memory>
#include "ApplicationSettings.h"
#include "CommonTypes.h"
#include "SignerDefs.h"
#include "TransactionsWidgetInterface.h"

namespace spdlog {
   class logger;
}
namespace Ui {
    class PortfolioWidget;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
   class UTXOReservationManager;
}

class QAction;

class ApplicationSettings;
class ArmoryConnection;
class CCPortfolioModel;
class HeadlessContainer;
class MarketDataProvider;
class MDCallbacksQt;
class TransactionsViewModel;
class UnconfirmedTransactionFilter;

class PortfolioWidget : public TransactionsWidgetInterface
{
Q_OBJECT

public:
   PortfolioWidget(QWidget* parent = nullptr );
   ~PortfolioWidget() override;

   void init(const std::shared_ptr<spdlog::logger>&);
   void SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model);

   void shortcutActivated(ShortcutType s) override;
   void setAuthorized(bool authorized);

   void onMDConnected();
   void onMDDisconnected();
   void onMDUpdated(bs::network::Asset::Type, const QString& security
      , const bs::network::MDFields&);
   void onHDWallet(const bs::sync::WalletInfo&);
   void onHDWalletDetails(const bs::sync::HDWalletData&);
   void onWalletBalance(const bs::sync::WalletBalanceData&);
   void onBalance(const std::string& currency, double balance);
   void onEnvConfig(int);

signals:
   void needMdConnection(ApplicationSettings::EnvConfiguration);
   void needMdDisconnect();

private slots:
   void showTransactionDetails(const QModelIndex& index);
   void showContextMenu(const QPoint& point);

private:
   std::unique_ptr<Ui::PortfolioWidget> ui_;
   UnconfirmedTransactionFilter* filter_;
   std::shared_ptr<CCPortfolioModel>   portfolioModel_;
};

#endif // __PORFOLIO_WIDGET_H__
