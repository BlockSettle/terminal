/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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

#include "TabWithShortcut.h"

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
}

class QAction;

class ApplicationSettings;
class ArmoryConnection;
class CCPortfolioModel;
class MarketDataProvider;
class SignContainer;
class TransactionsViewModel;
class UnconfirmedTransactionFilter;

class PortfolioWidget : public TabWithShortcut
{
Q_OBJECT

public:
   PortfolioWidget(QWidget* parent = nullptr );
   ~PortfolioWidget() override;

   void SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model);

   void init(const std::shared_ptr<ApplicationSettings> &
             , const std::shared_ptr<MarketDataProvider> &
             , const std::shared_ptr<CCPortfolioModel> &
             , const std::shared_ptr<SignContainer> &
             , const std::shared_ptr<ArmoryConnection> &
             , const std::shared_ptr<spdlog::logger> &
             , const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

   void setAuthorized(bool authorized);

private slots:
   void showTransactionDetails(const QModelIndex& index);
   void showContextMenu(const QPoint& point);

   void onCreateRBFDialog();
   void onCreateCPFPDialog();

private:
   std::unique_ptr<Ui::PortfolioWidget> ui_;
   std::shared_ptr<TransactionsViewModel> model_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   UnconfirmedTransactionFilter* filter_;
   QMenu    contextMenu_;
   QAction  *actionRBF_;
   QAction  *actionCPFP_;
   QAction  *actionCopyAddr_;
   QAction  *actionCopyTx_;
   QString  curAddress_;
   QString  curTx_;
   std::shared_ptr<SignContainer>      signContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
};

#endif // __PORFOLIO_WIDGET_H__
