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
      colOutput,
      colFees,
      colFeePerByte,
      colSize
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

   Q_INVOKABLE void getTxData(AsyncClient::LedgerDelegate inDelegate);

   Ui::AddressDetailsWidget *ui_; // The main widget object.
   bs::Address addrVal; // The address passed in by the user.
   std::string dummyWalletID; // The wallet ID.
   bs::PlainWallet dummyWallet; // Wallet that will hold the address.
   std::map<BinaryData, Tx> txMap_; // A wallet's Tx hash / Tx map.
   std::map<BinaryData, Tx> prevTxMap_; // A wallet's previous Tx hash / Tx map (fee stuff).
   std::map<BinaryData, bs::TXEntry> txEntryHashSet_; // A wallet's Tx hash / Tx entry map.
   std::set<BinaryData> txHashSet; // Hashes for a given address.
   std::set<BinaryData> prevTxHashSet; // Prev Tx hashes for an addr (fee calc).

   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // ADDRESSDETAILSWIDGET_H
