#include "TransactionDetailDialog.h"
#include "ui_TransactionDetailDialog.h"
#include "TransactionsViewModel.h"

#include <BTCNumericTypes.h>
#include <TxClasses.h>
#include "Wallets/SyncWalletsManager.h"
#include "UiUtils.h"

#include <QDateTime>
#include <QLabel>
#include <QDebug>
#include <QMenu>
#include <QClipboard>

#include <spdlog/spdlog.h>

#include <limits>


TransactionDetailDialog::TransactionDetailDialog(TransactionsViewItem tvi
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<ArmoryConnection> &armory, QWidget* parent)
 : QDialog(parent)
 , ui_(new Ui::TransactionDetailDialog())
 , walletsManager_(walletsManager)
{
   ui_->setupUi(this);

   const auto &cbInit = [this, armory] (const TransactionsViewItem *item) {
      ui_->labelAmount->setText(item->amountStr);
      ui_->labelDirection->setText(tr(bs::sync::Transaction::toString(item->direction)));
      ui_->labelAddress->setText(item->mainAddress);

      if (item->confirmations > 0) {
         ui_->labelHeight->setText(QString::number(item->txEntry.blockNum));
      }
      else {
         if (item->txEntry.isRBF) {
            ui_->labelFlag->setText(tr("RBF eligible"));
         } else if (item->isCPFP) {
            ui_->labelFlag->setText(tr("CPFP eligible"));
         }
      }

      if (item->tx.isInitialized()) {
         ui_->labelSize->setText(QString::number(item->tx.getTxWeight()));

         std::set<BinaryData> txHashSet;
         std::map<BinaryData, std::set<uint32_t>> txOutIndices;

         for (size_t i = 0; i < item->tx.getNumTxIn(); ++i) {
            TxIn in = item->tx.getTxInCopy(i);
            OutPoint op = in.getOutPoint();
            txHashSet.insert(op.getTxHash());
            txOutIndices[op.getTxHash()].insert(op.getTxOutIndex());
         }
         const auto &cbTXs = [this, item, txOutIndices](std::vector<Tx> txs) {
            itemSender = new QTreeWidgetItem(QStringList(tr("Sender")));
            itemReceiver = new QTreeWidgetItem(QStringList(tr("Receiver")));
            for (auto item : { itemSender, itemReceiver }) {
               ui_->treeAddresses->addTopLevelItem(item);
            }

            const auto &wallet = item->wallet;
            uint64_t value = 0;
            bool initialized = true;

            bool isTxOutgoing = false;
            switch (item->direction) {
            case bs::sync::Transaction::Sent:
            case bs::sync::Transaction::PayOut:
            case bs::sync::Transaction::Revoke:
               isTxOutgoing = true;
               break;

            case bs::sync::Transaction::Delivery:
            case bs::sync::Transaction::Payment:
            case bs::sync::Transaction::Auth:
               isTxOutgoing = (item->amount < 0);
               break;

            default:
               break;
            }

            for (const auto &prevTx : txs) {
               if (!prevTx.isInitialized()) {
                  continue;
               }
               const auto &itTxOut = txOutIndices.find(prevTx.getThisHash());
               if (itTxOut == txOutIndices.end()) {
                  continue;
               }
               for (const auto &txOutIdx : itTxOut->second) {
                  if (prevTx.isInitialized() && wallet) {
                     TxOut prevOut = prevTx.getTxOutCopy(txOutIdx);
                     value += prevOut.getValue();
                     addAddress(wallet, prevOut, false, isTxOutgoing,
                                prevTx.getThisHash());
                  }
                  else {
                     QStringList items;
                     items << tr("Input") << tr("???") << tr("Unknown");
                     itemSender->addChild(new QTreeWidgetItem(items));
                     initialized = false;
                  }
               }
            }

            if (wallet) {
               for (size_t i = 0; i < item->tx.getNumTxOut(); ++i) {
                  TxOut out = item->tx.getTxOutCopy(i);
                  value -= out.getValue();
                  addAddress(wallet, out, true, isTxOutgoing,
                             item->tx.getThisHash());
               }
               ui_->labelComment->setText(QString::fromStdString(wallet->getTransactionComment(item->tx.getThisHash())));
            }

            if (initialized) {
               ui_->labelFee->setText(UiUtils::displayAmount(value));
               ui_->labelSb->setText(
                  QString::number((float)value / (float)item->tx.getTxWeight()));
            }

            ui_->treeAddresses->expandItem(itemSender);
            ui_->treeAddresses->expandItem(itemReceiver);

            for (int i = 0; i < ui_->treeAddresses->columnCount(); ++i) {
               ui_->treeAddresses->resizeColumnToContents(i);
               ui_->treeAddresses->setColumnWidth(i,
                  ui_->treeAddresses->columnWidth(i) + extraTreeWidgetColumnMargin);
            }
            adjustSize();
         };
         if (txHashSet.empty()) {
            cbTXs({});
         }
         else {
            armory->getTXsByHash(txHashSet, cbTXs);
         }
      }

      ui_->labelConfirmations->setText(QString::number(item->confirmations));
   };
   tvi.initialize(armory, walletsManager, cbInit);

   bool bigEndianHash = true;
   ui_->labelHash->setText(QString::fromStdString(tvi.txEntry.txHash.toHexStr(bigEndianHash)));
   ui_->labelTime->setText(UiUtils::displayDateTime(QDateTime::fromTime_t(tvi.txEntry.txTime)));

   ui_->labelWalletName->setText(tvi.walletName.isEmpty() ? tr("Unknown") : tvi.walletName);

   /* disabled the context menu for copy to clipboard functionality, it can be removed later
   ui_->treeAddresses->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui_->treeAddresses, &QTreeView::customContextMenuRequested, [=](const QPoint& p) {
      const auto address = ui_->treeAddresses->itemAt(p)->data(0, Qt::UserRole).toString();

      if (!address.isEmpty()) {
         QMenu* menu = new QMenu(this);
         QAction* copyAction = menu->addAction(tr("&Copy Address"));
         connect(copyAction, &QAction::triggered, [=]() {
            qApp->clipboard()->setText(address);
         });
         menu->popup(ui_->treeAddresses->mapToGlobal(p));
      }
   });*/
   // allow address column to be copied to clipboard with right click
   ui_->treeAddresses->copyToClipboardColumns_.append(2);

   setMinimumHeight(minHeightAtRendering);
   resize(minimumSize());
}

TransactionDetailDialog::~TransactionDetailDialog() = default;

QSize TransactionDetailDialog::minimumSize() const
{
   int minWidth = 2 * extraTreeWidgetColumnMargin;

   for(int i = 0; i < ui_->treeAddresses->columnCount(); ++i) {
      minWidth += ui_->treeAddresses->columnWidth(i) + extraTreeWidgetColumnMargin;
   }

   return QSize(minWidth, minimumHeight());
}

QSize TransactionDetailDialog::minimumSizeHint() const
{
   return minimumSize();
}

// Add an address to the dialog.
// IN:  The wallet to check the address against. (const std::shared_ptr<bs::Wallet>)
//      The TxOut to check the address against. (const TxOut&)
//      Indicator for whether the TxOut is sourced against output. (bool)
//      Indicator for whether the Tx type is outgoing. (bool)
//      The TX hash. (const BinaryData&)
// OUT: None
// RET: None
void TransactionDetailDialog::addAddress(const std::shared_ptr<bs::sync::Wallet> &wallet,
                                         const TxOut& out,
                                         bool isOutput,
                                         bool isTxOutgoing,
                                         const BinaryData& txHash)
{
   const auto addr = bs::Address::fromTxOut(out);
   const auto &addressWallet = walletsManager_->getWalletByAddress(addr.id());
   QString valueStr;
   QString addressType;
   const bool isOurs = (addressWallet == wallet);

   if (isTxOutgoing) {
      const bool isSettlement = (wallet->type() == bs::core::wallet::Type::Settlement);
      if (isOurs) {
         if (!isOutput) {
            valueStr += QLatin1Char('-');
         }
         else if (!isSettlement) {
            addressType = tr("Change");
            isOutput = false;
         }
      }
   }
   else {
      if (!isOurs) {
         if (!isOutput) {
            valueStr += QLatin1Char('-');
         }
         else {
            addressType = tr("Change");
         }
         isOutput = false;
      }
   }
   valueStr += addressWallet ? addressWallet->displayTxValue(out.getValue()) : UiUtils::displayAmount(out.getValue());
   if (addressType.isEmpty()) {
      addressType = isOutput ? tr("Output") : tr("Input");
   }

   QString walletName = addressWallet ? QString::fromStdString(addressWallet->name()) : QString();
   QStringList items;
   items << addressType;
   items << valueStr;
   const auto displayedAddress = addr.display();
   if (walletName.isEmpty()) {
      items << displayedAddress;
   }
   else {
      items << displayedAddress << walletName;
   }

   auto parent = isOutput ? itemReceiver : itemSender;
   auto item = new QTreeWidgetItem(items);
   item->setData(0, Qt::UserRole, displayedAddress);
   const auto txHashStr = QString::fromStdString(txHash.toHexStr(true));
   auto txItem = new QTreeWidgetItem(QStringList() << getScriptType(out)
                                     << QString::number(out.getValue())
                                     << txHashStr);
   txItem->setData(0, Qt::UserRole, txHashStr);
   item->addChild(txItem);
   parent->addChild(item);
}

QString TransactionDetailDialog::getScriptType(const TxOut &out)
{
   switch (out.getScriptType()) {
   case TXOUT_SCRIPT_STDHASH160:    return tr("hash160");
   case TXOUT_SCRIPT_STDPUBKEY65:   return tr("pubkey65");
   case TXOUT_SCRIPT_STDPUBKEY33:   return tr("pubkey33");
   case TXOUT_SCRIPT_MULTISIG:      return tr("multisig");
   case TXOUT_SCRIPT_P2SH:          return tr("p2sh");
   case TXOUT_SCRIPT_NONSTANDARD:   return tr("non-std");
   case TXOUT_SCRIPT_P2WPKH:        return tr("p2wpkh");
   case TXOUT_SCRIPT_P2WSH:         return tr("p2wsh");
   default:            return tr("unknown");
   }
}
