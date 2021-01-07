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

   void SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model);

   void init(const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<MarketDataProvider> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<CCPortfolioModel> &
      , const std::shared_ptr<HeadlessContainer> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
      , const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

   void setAuthorized(bool authorized);

private slots:
   void showTransactionDetails(const QModelIndex& index);
   void showContextMenu(const QPoint& point);

private:
   std::unique_ptr<Ui::PortfolioWidget> ui_;
   UnconfirmedTransactionFilter* filter_;
};

#endif // __PORFOLIO_WIDGET_H__
