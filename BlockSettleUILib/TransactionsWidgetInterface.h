/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef _TRANSACTIONS_WIDGET_INTERFACE_
#define _TRANSACTIONS_WIDGET_INTERFACE_

#include "TabWithShortcut.h"
#include <set>
#include <QMenu>
#include "BinaryData.h"
#include "BSErrorCode.h"

namespace spdlog {
   class logger;
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
class TransactionsViewModel;

class TransactionsWidgetInterface : public TabWithShortcut {
   Q_OBJECT
public:
   explicit TransactionsWidgetInterface(QWidget *parent = nullptr);
   ~TransactionsWidgetInterface() noexcept override = default;

   void init(const std::shared_ptr<spdlog::logger> &);

protected slots:
   void onRevokeSettlement();
   void onCreateRBFDialog();
   void onCreateCPFPDialog();
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode, std::string error);

protected:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<HeadlessContainer>     signContainer_;
   std::shared_ptr<ArmoryConnection>      armory_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<TransactionsViewModel> model_;

   std::set<unsigned int>  revokeIds_;

   QMenu    contextMenu_;
   QAction  *actionCopyAddr_ = nullptr;
   QAction  *actionCopyTx_ = nullptr;
   QAction  *actionRBF_ = nullptr;
   QAction  *actionCPFP_ = nullptr;
   QAction  *actionRevoke_ = nullptr;
   QString  curAddress_;
   QString  curTx_;
};

#endif // _TRANSACTIONS_WIDGET_INTERFACE_
