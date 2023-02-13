/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TRANSACTIONS_WIDGET_UI_H__
#define __TRANSACTIONS_WIDGET_UI_H__

#include <memory>
#include <set>
#include <QMenu>
#include <QWidget>
#include "BinaryData.h"
#include "BSErrorCode.h"
#include "TransactionsWidgetInterface.h"
#include "Wallets/SignerDefs.h"

namespace spdlog {
   class logger;
}
namespace Ui {
   class TransactionsWidget;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
   class UTXOReservationManager;
}
class ApplicationSettings;
class ArmoryConnection;
class HeadlessContainer;
class TransactionsProxy;
class TransactionsViewModel;
class TransactionsSortFilterModel;


class TransactionsWidget : public TransactionsWidgetInterface
{
Q_OBJECT

public:
   TransactionsWidget(QWidget* parent = nullptr );
   ~TransactionsWidget() override;

   void init(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<TransactionsViewModel> &);

   void shortcutActivated(ShortcutType s) override;

   void onHDWalletDetails(const bs::sync::HDWalletData&);
   void onWalletDeleted(const bs::sync::WalletInfo&);

private slots:
   void showTransactionDetails(const QModelIndex& index);
   void updateResultCount();
   void walletsChanged();
   void walletsFilterChanged(int index);
   void onEnterKeyInTrxPressed(const QModelIndex &index);
   void onDataLoaded(int count);
   void onProgressInited(int start, int end);
   void onProgressUpdated(int value);

private:
   void scheduleDateFilterCheck();
   std::unique_ptr<Ui::TransactionsWidget> ui_;
   std::unordered_map<std::string, bs::sync::HDWalletData>   wallets_;

   TransactionsSortFilterModel         *  sortFilterModel_;
};


#endif // __TRANSACTIONS_WIDGET_UI_H__
