#ifndef __PORFOLIO_WIDGET_H__
#define __PORFOLIO_WIDGET_H__

#include <QWidget>
#include <QMenu>
#include <memory>

#include "TabWithShortcut.h"

namespace Ui {
    class PortfolioWidget;
};

class QAction;

class ApplicationSettings;
class ArmoryConnection;
class CCPortfolioModel;
class MarketDataProvider;
class SignContainer;
class TransactionsViewModel;
class UnconfirmedTransactionFilter;
class WalletsManager;

class PortfolioWidget : public TabWithShortcut
{
Q_OBJECT

public:
   PortfolioWidget(QWidget* parent = nullptr );
   ~PortfolioWidget() override = default;

   void SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model);

   void init(const std::shared_ptr<ApplicationSettings> &, const std::shared_ptr<MarketDataProvider> &
      , const std::shared_ptr<CCPortfolioModel> &, const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ArmoryConnection> &, const std::shared_ptr<WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

private slots:
   void showTransactionDetails(const QModelIndex& index);
   void showContextMenu(const QPoint& point);

   void onCreateRBFDialog();
   void onCreateCPFPDialog();
private:
   Ui::PortfolioWidget* ui_;
   std::shared_ptr<TransactionsViewModel> model_;
   UnconfirmedTransactionFilter* filter_;
   QMenu    contextMenu_;
   QAction  *actionRBF_;
   QAction  *actionCPFP_;
   std::shared_ptr<SignContainer>      signContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<WalletsManager>     walletsManager_;
};

#endif // __PORFOLIO_WIDGET_H__
