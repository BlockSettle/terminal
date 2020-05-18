/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
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
#include "TabWithShortcut.h"

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
class TransactionsProxy;
class TransactionsViewModel;
class TransactionsSortFilterModel;
class WalletSignerContainer;


class TransactionsWidget : public TabWithShortcut
{
Q_OBJECT

public:
   TransactionsWidget(QWidget* parent = nullptr );
   ~TransactionsWidget() override;

   void init(const std::shared_ptr<bs::sync::WalletsManager> &
             , const std::shared_ptr<ArmoryConnection> &
             , const std::shared_ptr<bs::UTXOReservationManager> &
             , const std::shared_ptr<WalletSignerContainer> &
             , const std::shared_ptr<ApplicationSettings>&
             , const std::shared_ptr<spdlog::logger> &);
   void SetTransactionsModel(const std::shared_ptr<TransactionsViewModel> &);

   void shortcutActivated(ShortcutType s) override;

private slots:
   void showTransactionDetails(const QModelIndex& index);
   void updateResultCount();
   void walletsChanged();
   void walletsFilterChanged(int index);
   void onEnterKeyInTrxPressed(const QModelIndex &index);
   void onDataLoaded(int count);
   void onCreateRBFDialog();
   void onCreateCPFPDialog();
   void onProgressInited(int start, int end);
   void onProgressUpdated(int value);
   void onRevokeSettlement();
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode, std::string error);

private:
   void scheduleDateFilterCheck();

   std::unique_ptr<Ui::TransactionsWidget> ui_;
   std::shared_ptr<spdlog::logger>     logger_;

   std::shared_ptr<TransactionsViewModel> transactionsModel_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<WalletSignerContainer> signContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   TransactionsSortFilterModel         *  sortFilterModel_;
   QMenu    contextMenu_;
   QAction  *actionCopyAddr_ = nullptr;
   QAction  *actionCopyTx_ = nullptr;
   QAction  *actionRBF_ = nullptr;
   QAction  *actionCPFP_ = nullptr;
   QAction  *actionRevoke_ = nullptr;
   QString  curAddress_;
   QString  curTx_;
   std::set<unsigned int>  revokeIds_;
};


#endif // __TRANSACTIONS_WIDGET_UI_H__
