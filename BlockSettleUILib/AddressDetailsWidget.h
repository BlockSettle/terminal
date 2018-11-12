#ifndef __ADDRESSDETAILSWIDGET_H__
#define __ADDRESSDETAILSWIDGET_H__

#include "Address.h"
#include "ArmoryConnection.h"
#include "PlainWallet.h"
#include "WalletsManager.h"

#include <QWidget>
#include <QItemSelection>

namespace Ui {
   class AddressDetailsWidget;
}

class QTreeWidgetItem;

class AddressDetailsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit AddressDetailsWidget(QWidget *parent = nullptr);
   ~AddressDetailsWidget();

   void init(const std::shared_ptr<ArmoryConnection> &armory,
             const std::shared_ptr<spdlog::logger> &inLogger);
   void setAddrVal(const bs::Address& inAddrVal);
   void loadWallet();
   void loadTransactions();

   enum AddressTreeColumns {
      colDate = 0,
      colTxId = 1,
      colConfs = 2,
      colInputsNum,
      colOutputsNum,
      colOutputAmt,
      colFees,
      colFeePerByte,
      colTxSize
   };

signals:
   void transactionClicked(QString txId);

protected slots:
   void onTxClicked(QTreeWidgetItem *item, int column);

private slots:
   void OnRefresh(std::vector<BinaryData> ids);

private:
   void setConfirmationColor(QTreeWidgetItem *item);
   void setOutputColor(QTreeWidgetItem *item);
   void getTxData(AsyncClient::LedgerDelegate inDelegate);

   // NB: Right now, the code is slightly inefficient. There are two maps with
   // hashes for keys. One has transactions (Armory), and TXEntry objects (BS).
   // This is due to the manner in which we retrieve data from Armory. Pages are
   // returned for addresses, and we then retrieve the appropriate Tx objects
   // from Armory. (Tx searches go directly to Tx object retrieval.) The thing
   // is that the pages are what have data related to # of confs and other
   // block-related data. The Tx objects from Armory don't have block-related
   // data that we need. So, we need two maps, at least for now.

   Ui::AddressDetailsWidget *ui_; // The main widget object.
   bs::Address addrVal_; // The address passed in by the user.
   std::string dummyWalletID_; // The wallet ID.
   bs::PlainWallet dummyWallet_; // Wallet that will hold the address.
   std::map<BinaryData, Tx> txMap_; // A wallet's Tx hash / Tx map.
   std::map<BinaryData, Tx> prevTxMap_; // A wallet's previous Tx hash / Tx map (fee calc).
   std::map<BinaryData, bs::TXEntry> txEntryHashSet_; // A wallet's Tx hash / Tx entry map.
   std::set<BinaryData> txHashSet_; // Hashes for a given address.
   std::set<BinaryData> prevTxHashSet_; // Prev Tx hashes for an addr (fee calc).

   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // ADDRESSDETAILSWIDGET_H
