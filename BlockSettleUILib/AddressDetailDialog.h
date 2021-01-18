/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ADDRESS_DETAIL_DIALOG_H__
#define __ADDRESS_DETAIL_DIALOG_H__

#include <QDialog>
#include <memory>
#include "Address.h"
#include "AsyncClient.h"
#include "CoreWallet.h"
#include "SignerDefs.h"

namespace spdlog {
   class logger;
}
namespace Ui {
   class AddressDetailDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}
class ArmoryConnection;
class TransactionsViewModel;

class AddressDetailDialog : public QDialog
{
Q_OBJECT

public:
   AddressDetailDialog(const bs::Address &, const std::shared_ptr<spdlog::logger> &
      , bs::core::wallet::Type, uint64_t balance, uint32_t txn
      , const QString &walletName, const std::string &addrIndex
      , const std::string &comment, QWidget* parent = nullptr);
   ~AddressDetailDialog() override;

   void onNewBlock(unsigned int blockNum);
   void onLedgerEntries(uint32_t curBlock, const std::vector<bs::TXEntry> &);
   void onTXDetails(const std::vector<bs::sync::TXWalletDetails> &);

signals:
   void needTXDetails(const std::vector<bs::sync::TXWallet> &, bool useCache
      , const bs::Address &);

private slots:
   void onCopyClicked() const;
   void onAddrTxNReceived(uint32_t);
   void onInputAddrContextMenu(const QPoint &pos);
   void onOutputAddrContextMenu(const QPoint &pos);

private:
   void setBalance(uint64_t, bs::core::wallet::Type);
   void onError();

private:
   std::unique_ptr <Ui::AddressDetailDialog> ui_;
   const bs::Address                   address_;
   std::shared_ptr<spdlog::logger>     logger_;
   TransactionsViewModel   *model_{ nullptr };
};

#endif // __ADDRESS_DETAIL_DIALOG_H__
