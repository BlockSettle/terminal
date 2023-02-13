/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TransactionDetailDialog.h"
#include "ui_TransactionDetailDialog.h"
#include "TransactionsViewModel.h"

#include <BTCNumericTypes.h>
#include <TxClasses.h>
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "UiUtils.h"

#include <QDateTime>
#include <QLabel>
#include <QMenu>
#include <QClipboard>

#include <spdlog/spdlog.h>

#include <limits>


TransactionDetailDialog::TransactionDetailDialog(const TransactionPtr &txi
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::TransactionDetailDialog())
{
   ui_->setupUi(this);
   itemSender_ = new QTreeWidgetItem(QStringList(tr("Sender")));
   itemReceiver_ = new QTreeWidgetItem(QStringList(tr("Receiver")));

   ui_->labelAmount->setText(txi->amountStr);
   ui_->labelDirection->setText(tr(bs::sync::Transaction::toString(txi->direction)));
   ui_->labelAddress->setText(txi->mainAddress);

   if (txi->confirmations > 0) {
      ui_->labelHeight->setText(QString::number(txi->txEntry.blockNum));
   } else {
      if (txi->txEntry.isRBF) {
         ui_->labelFlag->setText(tr("RBF eligible"));
      } else if (txi->isCPFP) {
         ui_->labelFlag->setText(tr("CPFP eligible"));
      }
   }

   ui_->treeAddresses->addTopLevelItem(itemSender_);
   ui_->treeAddresses->addTopLevelItem(itemReceiver_);
   ui_->labelComment->setText(txi->comment);

   if (txi->tx.isInitialized()) {
      ui_->labelSize->setText(QString::number(txi->tx.getTxWeight()));
   }

   int64_t value = 0;
   for (const auto &addrDet : txi->inputAddresses) {
      addInputAddress(addrDet);
      value += addrDet.value;
   }
   if (!txi->changeAddress.address.empty()) {
      addChangeAddress(txi->changeAddress);
      value -= txi->changeAddress.value;
   }
   for (const auto &addrDet : txi->outputAddresses) {
      addOutputAddress(addrDet);
      value -= addrDet.value;
   }

   ui_->labelFee->setText(UiUtils::displayAmount(value));
   ui_->labelSb->setText(
      QString::number((float)value / (float)txi->tx.getTxWeight()));

   for (int i = 0; i < ui_->treeAddresses->columnCount(); ++i) {
      ui_->treeAddresses->resizeColumnToContents(i);
      ui_->treeAddresses->setColumnWidth(i,
         ui_->treeAddresses->columnWidth(i) + extraTreeWidgetColumnMargin);
   }

   ui_->treeAddresses->expandItem(itemSender_);
   ui_->treeAddresses->expandItem(itemReceiver_);
   adjustSize();

   ui_->labelConfirmations->setText(QString::number(txi->confirmations));

   const bool bigEndianHash = true;
   ui_->labelHash->setText(QString::fromStdString(txi->txEntry.txHash.toHexStr(bigEndianHash)));
   ui_->labelTime->setText(UiUtils::displayDateTime(QDateTime::fromTime_t(txi->txEntry.txTime)));

   ui_->labelWalletName->setText(txi->walletName.isEmpty() ? tr("Unknown") : txi->walletName);

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
// IN:  The TxOut to check the address against. (const TxOut&)
//      Indicator for whether the TxOut is sourced against output. (bool)
//      Indicator for whether the Tx type is outgoing. (bool)
//      The TX hash. (const BinaryData&)
// OUT: None
// RET: None
void TransactionDetailDialog::addAddress(TxOut out    // can't use const ref due to TxOut::getIndex()
   , bool isOutput, const BinaryData& txHash
   , const WalletsSet &inputWallets, const std::vector<TxOut> &allOutputs)
{
   QString addressType;
   QString displayedAddress;
   bs::sync::WalletsManager::WalletPtr addressWallet;
   bool isChange = false;
   try {
      const auto &addr = bs::Address::fromTxOut(out);
      addressWallet = walletsManager_->getWalletByAddress(addr);

      TxOut lastChange;
      const auto &setLastChange = [&lastChange, inputWallets, this]
         (TxOut out)
      {
         const auto &addr = bs::Address::fromTxOut(out);
         const auto &addrWallet = walletsManager_->getWalletByAddress(addr);
         if (inputWallets.find(addrWallet) != inputWallets.end()) {
            lastChange = out;
         }
      };

      if (isOutput) {
         if (!allOutputs.empty()) {
            for (auto output : allOutputs) {
               setLastChange(output);
            }
         } else {
            setLastChange(out);
         }

         if (lastChange.isInitialized() && (allOutputs.size() > 1)
            && (out.getScript() == lastChange.getScript())) {
            isChange = true;
         }
      }

      addressType = isChange ? tr("Change") : (isOutput ? tr("Output") : tr("Input"));
      displayedAddress = QString::fromStdString(addr.display());
   }
   catch (const std::exception &) {    // Likely OP_RETURN
      addressType = tr("Unknown");
      displayedAddress = tr("empty");
   }

   // Inputs should be negative, outputs positive, and change positive
   QString valueStr = isOutput ? QString() : QLatin1String("-");
   QString walletName;

   const auto parent = (!isOutput || isChange) ? itemSender_ : itemReceiver_;

   if (addressWallet) {
      valueStr += addressWallet->displayTxValue(int64_t(out.getValue()));
      walletName = QString::fromStdString(addressWallet->name());
   }
   else {
      valueStr += UiUtils::displayAmount(out.getValue());
   }
   QStringList items;
   items << addressType;
   items << valueStr;
   if (walletName.isEmpty()) {
      items << displayedAddress;
   } else {
      items << displayedAddress << walletName;
   }

   auto item = new QTreeWidgetItem(items);
   item->setData(0, Qt::UserRole, displayedAddress);
   item->setData(1, Qt::UserRole, (qulonglong)out.getValue());
   parent->addChild(item);
   const auto txHashStr = QString::fromStdString(fmt::format("{}/{}", txHash.toHexStr(true), out.getIndex()));
   auto txItem = new QTreeWidgetItem(QStringList() << getScriptType(out)
                                     << QString::number(out.getValue())
                                     << txHashStr);
   txItem->setData(0, Qt::UserRole, txHashStr);
   item->addChild(txItem);
}

void TransactionDetailDialog::addInputAddress(const bs::sync::AddressDetails &addrDet)
{
   const auto &addressType = tr("Input");
   const auto &displayedAddress = QString::fromStdString(addrDet.address.display());
   const auto &valueStr = QString::fromStdString(addrDet.valueStr);
   QStringList items;
   items << addressType << valueStr << displayedAddress << QString::fromStdString(addrDet.walletName);

   auto item = new QTreeWidgetItem(items);
   item->setData(0, Qt::UserRole, displayedAddress);
   item->setData(1, Qt::UserRole, (qulonglong)addrDet.value);
   itemSender_->addChild(item);
   const auto &txHashStr = QString::fromStdString(fmt::format("{}/{}"
      , addrDet.outHash.toHexStr(true), addrDet.outIndex));
   auto txItem = new QTreeWidgetItem(QStringList() << getScriptType(addrDet.type)
      << QString::number(addrDet.value) << txHashStr);
   txItem->setData(0, Qt::UserRole, txHashStr);
   item->addChild(txItem);
}

void TransactionDetailDialog::addChangeAddress(const bs::sync::AddressDetails &addrDet)
{
   const auto &addressType = tr("Change");
   const auto &displayedAddress = QString::fromStdString(addrDet.address.display());
   const auto &valueStr = QString::fromStdString(addrDet.valueStr);
   QStringList items;
   items << addressType << valueStr << displayedAddress << QString::fromStdString(addrDet.walletName);

   auto item = new QTreeWidgetItem(items);
   item->setData(0, Qt::UserRole, displayedAddress);
   item->setData(1, Qt::UserRole, (qulonglong)addrDet.value);
   itemSender_->addChild(item);
   const auto &txHashStr = QString::fromStdString(fmt::format("{}/{}"
      , addrDet.outHash.toHexStr(true), addrDet.outIndex));
   auto txItem = new QTreeWidgetItem(QStringList() << getScriptType(addrDet.type)
      << QString::number(addrDet.value) << txHashStr);
   txItem->setData(0, Qt::UserRole, txHashStr);
   item->addChild(txItem);
}

void TransactionDetailDialog::addOutputAddress(const bs::sync::AddressDetails &addrDet)
{
   const auto &addressType = tr("Output");
   const auto &displayedAddress = QString::fromStdString(addrDet.address.display());
   const auto &valueStr = QString::fromStdString(addrDet.valueStr);
   QStringList items;
   items << addressType << valueStr << displayedAddress << QString::fromStdString(addrDet.walletName);

   auto item = new QTreeWidgetItem(items);
   item->setData(0, Qt::UserRole, displayedAddress);
   item->setData(1, Qt::UserRole, (qulonglong)addrDet.value);
   itemReceiver_->addChild(item);
   const auto &txHashStr = QString::fromStdString(fmt::format("{}/{}"
      , addrDet.outHash.toHexStr(true), addrDet.outIndex));
   auto txItem = new QTreeWidgetItem(QStringList() << getScriptType(addrDet.type)
      << QString::number(addrDet.value) << txHashStr);
   txItem->setData(0, Qt::UserRole, txHashStr);
   item->addChild(txItem);
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
      case TXOUT_SCRIPT_OPRETURN:      return tr("op-return");
   }
   return tr("unknown");
}

QString TransactionDetailDialog::getScriptType(TXOUT_SCRIPT_TYPE scrType)
{
   switch (scrType) {
   case TXOUT_SCRIPT_STDHASH160:    return tr("hash160");
   case TXOUT_SCRIPT_STDPUBKEY65:   return tr("pubkey65");
   case TXOUT_SCRIPT_STDPUBKEY33:   return tr("pubkey33");
   case TXOUT_SCRIPT_MULTISIG:      return tr("multisig");
   case TXOUT_SCRIPT_P2SH:          return tr("p2sh");
   case TXOUT_SCRIPT_NONSTANDARD:   return tr("non-std");
   case TXOUT_SCRIPT_P2WPKH:        return tr("p2wpkh");
   case TXOUT_SCRIPT_P2WSH:         return tr("p2wsh");
   case TXOUT_SCRIPT_OPRETURN:      return tr("op-return");
   }
   return tr("unknown");
}
