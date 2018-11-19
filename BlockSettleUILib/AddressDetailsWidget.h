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
   void clearFields();

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
   //
   // In addition, note that the TX hashes returned by Armory are in "internal"
   // byte order, whereas the displayed values need to be in "RPC" byte order.
   // (Look at the BinaryTXID class comments for more info on this phenomenon.)
   // The only time we care about this is when displaying data to the user; the
   // data is consistent otherwise, which makes Armory happy. Don't worry about
   // about BinaryTXID. A simple endian flip in printed strings is all we need.

   Ui::AddressDetailsWidget *ui_; // The main widget object.
   bs::Address addrVal_; // The address passed in by the user.
   std::string dummyWalletID_; // The wallet ID.
   bs::PlainWallet dummyWallet_; // Wallet that will hold the address.
   std::map<BinaryData, Tx> txMap_; // A wallet's Tx hash / Tx map.
   std::map<BinaryData, Tx> prevTxMap_; // A wallet's previous Tx hash / Tx map (fee calc).
   std::map<BinaryData, bs::TXEntry> txEntryHashSet_; // A wallet's Tx hash / Tx entry map.

   // Structs passed to Armory to tell it which TX hashes to look up.
   std::set<BinaryData> txHashSet_; // Hashes assoc'd with a given address.
   std::set<BinaryData> prevTxHashSet_; // Prev Tx hashes for an addr (fee calc).

   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // ADDRESSDETAILSWIDGET_H
