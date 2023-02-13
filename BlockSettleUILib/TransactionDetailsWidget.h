/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TRANSACTIONDETAILSWIDGET_H__
#define __TRANSACTIONDETAILSWIDGET_H__

#include "ArmoryConnection.h"
#include "BinaryData.h"
#include "Wallets/SignerDefs.h"
#include "TxClasses.h"

#include <QMap>
#include <QWidget>

// Important note: The concept of endianness doesn't really apply to transaction
// IDs (TXIDs). This is due to how SHA-256 works. Instead, Bitcoin Core uses an
// "internal" byte order for calculation purposes and a byte-flipped "RPC" order
// when responding to remote queries. In turn, virtually all block explorers use
// RPC order, and so does the terminal. However, Armory expects internal-ordered
// TXIDs. Always assume internal ordering with Armory calls! (All that said, the
// internal order is argably little endian since Core was originally written for
// x86. This, in turn, would make RPC order big endian.)

namespace spdlog {
   class logger;
}
namespace Ui {
   class TransactionDetailsWidget;
}
namespace bs {
   namespace sync {
      class CCDataResolver;
      class WalletsManager;
   }
}
class CustomTreeWidget;
class QTreeWidgetItem;
class QTreeWidget;

// Create a class that takes a TXID and handles the data based on whether we're
// dealing with internal or RPC order. Not 100% robust for now (this assumes
// little endian is used) but it'll suffice until we require more robust data
// handling.
class TxHash : public BinaryData {
public:
   TxHash(const BinaryData &bd) : BinaryData(bd) {}
/*   TxHash(const QByteArray &txidData) :
      BinaryData((uint8_t*)(txidData.data()), txidData.size()) {}*/
   TxHash(const QString &txidData) : BinaryData(READHEX(txidData.toStdString()))
   {  // imply hex data is always in RPC format
      swapEndian();
   }
   TxHash(const std::string &txidData) : BinaryData(READHEX(txidData.data()))
   {  // imply hex data is always in RPC format
      swapEndian();
   }

   // Make two separate functs just to make internal vs. RPC clearer to devs.
   std::string getRPCTXID() const { return toHexStr(true); }
};

class TransactionDetailsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit TransactionDetailsWidget(QWidget *parent = nullptr);
   ~TransactionDetailsWidget() override;

   void init(const std::shared_ptr<spdlog::logger>&);

   void populateTransactionWidget(const TxHash &rpcTXID,
      const bool& firstPass = true);

    enum TxTreeColumns {
      colAddressId,
      colAmount,
      colWallet
    };

    void onTXDetails(const std::vector<bs::sync::TXWalletDetails>&);

public slots:
   void onNewBlock(unsigned int blockNum);

signals:
   void addressClicked(QString addressId);
   void txHashClicked(QString txHash);
   void finished() const;
   void needTXDetails(const std::vector<bs::sync::TXWallet>&, bool useCache
      , const bs::Address&);

protected slots:
   void onAddressClicked(QTreeWidgetItem *item, int column);

protected:
   void loadTreeIn(CustomTreeWidget *tree);
   void loadTreeOut(CustomTreeWidget *tree);

private:
   void loadInputs();
   void setTxGUIValues();
   void clear();

   void processTxData(const Tx &tx);

   void addItem(QTreeWidget *tree, const QString &address, const uint64_t amount
      , const QString &wallet, const BinaryData &txHash, const int txIndex = -1);

private:
   std::unique_ptr<Ui::TransactionDetailsWidget>   ui_;
   std::shared_ptr<ArmoryConnection>   armoryPtr_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;

   Tx curTx_; // The Tx being analyzed in the widget.
   BinaryData  curTxHash_;
   uint32_t    topBlock_{ 0 };

   // Data captured from Armory callbacks.
   AsyncClient::TxBatchResult prevTxMap_; // Prev Tx hash / Prev Tx map.

   class TxDetailsACT : public ArmoryCallbackTarget
   {
   public:
      TxDetailsACT(TransactionDetailsWidget *parent)
         : parent_(parent) {}
      ~TxDetailsACT() override { cleanup(); }
      void onNewBlock(unsigned int height, unsigned int) override {
         QMetaObject::invokeMethod(parent_, [this, height] { parent_->onNewBlock(height); });
      }
   private:
      TransactionDetailsWidget *parent_;
   };
   std::unique_ptr<TxDetailsACT> act_;
};

#endif // TRANSACTIONDETAILSWIDGET_H
