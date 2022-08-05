/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TransactionDetailsWidget.h"
#include "ui_TransactionDetailsWidget.h"
#include "BTCNumericTypes.h"
#include "BlockObj.h"
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <memory>
#include <QToolTip>

using namespace Armory::Signer;

Q_DECLARE_METATYPE(Tx);


TransactionDetailsWidget::TransactionDetailsWidget(QWidget *parent) :
    QWidget(parent),
   ui_(new Ui::TransactionDetailsWidget)
{
   ui_->setupUi(this);

   // setting up a tooltip that pops up immediately when mouse hovers over it
   QIcon btcIcon(QLatin1String(":/resources/notification_info.png"));
   ui_->labelTxPopup->setPixmap(btcIcon.pixmap(13, 13));
   ui_->labelTxPopup->setMouseTracking(true);
   ui_->labelTxPopup->toolTip_ = tr("The Transaction ID (TXID) uses RPC byte "
                                    "order. It will match the RPC output from "
                                    "Bitcoin Core, along with the byte order "
                                    "from the BlockSettle Terminal.");

   // set the address column to have hand cursor
   ui_->treeInput->handCursorColumns_.append(colAddressId);
   ui_->treeOutput->handCursorColumns_.append(colAddressId);
   // allow address columns to be copied to clipboard with right click
   ui_->treeInput->copyToClipboardColumns_.append(colAddressId);
   ui_->treeOutput->copyToClipboardColumns_.append(colAddressId);

   connect(ui_->treeInput, &QTreeWidget::itemClicked,
      this, &TransactionDetailsWidget::onAddressClicked);
   connect(ui_->treeOutput, &QTreeWidget::itemClicked,
      this, &TransactionDetailsWidget::onAddressClicked);
}

TransactionDetailsWidget::~TransactionDetailsWidget() = default;

void TransactionDetailsWidget::init(const std::shared_ptr<spdlog::logger> &logger)
{
   logger_ = logger;
}

// This function uses getTxByHash() to retrieve info about transaction. The
// incoming TXID must be in RPC order, not internal order.
void TransactionDetailsWidget::populateTransactionWidget(const TxHash &rpcTXID
   , const bool &firstPass)
{
   // In case we've been here earlier, clear all the text.
   if (firstPass) {
      clear();
   }

   if (!armoryPtr_) {
      if (rpcTXID.getSize() == 32) {
         curTxHash_ = rpcTXID;
         emit needTXDetails({ { curTxHash_, {}, 0 } }, false, {});
      }
      else {
         Codec_SignerState::SignerState signerState;
         if (signerState.ParseFromString(rpcTXID.toBinStr(true))) {
            Signer signer(signerState);
            const auto& serTx = signer.serializeUnsignedTx(true);
            try {
               const Tx tx(serTx);
               if (!tx.isInitialized()) {
                  throw std::runtime_error("Uninited TX");
               }
               curTxHash_ = tx.getThisHash();
               emit needTXDetails({ { tx.getThisHash(), {}, 0 } }, false, {});
            }
            catch (const std::exception& e) {
               logger_->error("[TransactionDetailsWidget::populateTransactionWidget]"
                  " {}", e.what());
            }
         }
         else {
            logger_->error("[TransactionDetailsWidget::populateTransactionWidget]"
               " failed to decode signer state");
         }
      }
      return;
   }

   // get the transaction data from armory
   const auto txidStr = rpcTXID.getRPCTXID();
   const auto cbTX = [this, txidStr](const Tx &tx) {
      if (!tx.isInitialized()) {
         if (logger_) {
            logger_->error("[TransactionDetailsWidget::cbTx] TXID {} is not inited"
               , txidStr);
         }
         ui_->tranID->setText(tr("%1 (load failed)").arg(QString::fromStdString(txidStr)));
         emit finished();
         return;
      }
      processTxData(tx);
   };

   if (firstPass || !curTx_.isInitialized() || (curTx_.getThisHash() != rpcTXID)) {
      if (rpcTXID.getSize() == 32) {
         if (!armoryPtr_->getTxByHash(rpcTXID, cbTX, false)) {
            if (logger_) {
               logger_->error("[TransactionDetailsWidget::populateTransactionWidget]"
                  " failed to get TXID {}", txidStr);
            }
         }
      }
      else {
         Codec_SignerState::SignerState signerState;
         if (signerState.ParseFromString(rpcTXID.toBinStr(true))) {
            Signer signer(signerState);
            const auto &serTx = signer.serializeUnsignedTx(true);
            try {
               const Tx tx(serTx);
               if (!tx.isInitialized()) {
                  throw std::runtime_error("Uninited TX");
               }
               cbTX(tx);
            }
            catch (const std::exception &e) {
               logger_->error("[TransactionDetailsWidget::populateTransactionWidget]"
                  " {}", e.what());
            }
         }
         else {
            logger_->error("[TransactionDetailsWidget::populateTransactionWidget]"
               " failed to decode signer state");
         }
      }
   }
   else {
      cbTX(curTx_);
   }
}

void TransactionDetailsWidget::onTXDetails(const std::vector<bs::sync::TXWalletDetails> &txDet)
{
   if (txDet.empty()) {
      return;
   }
   if ((txDet.size() > 1) || (!txDet.empty() && !txDet[0].txHash.empty() && (txDet[0].txHash != curTxHash_))) {
      logger_->debug("[{}] not our TX details", __func__);
      return;  // not our data
   }
   if ((txDet[0].txHash.empty() || !txDet[0].tx.getSize()) && !txDet[0].comment.empty()) {
      ui_->tranID->setText(QString::fromStdString(txDet[0].comment));
      return;
   }
   curTx_ = txDet[0].tx;
   if (!curTx_.isInitialized()) {
      ui_->tranID->setText(tr("Loading..."));
      return;
   }
   ui_->tranID->setText(QString::fromStdString(curTx_.getThisHash().toHexStr(true)));
   emit finished();

   const auto txHeight = curTx_.getTxHeight();
   ui_->nbConf->setVisible(txHeight != UINT32_MAX);
   ui_->labelNbConf->setVisible(txHeight != UINT32_MAX);
   const uint32_t nbConf = topBlock_ ? topBlock_ + 1 - txHeight : 0;
   ui_->nbConf->setText(QString::number(nbConf));

   if (txDet.empty()) {
      return;
   }
   // Get fees & fee/byte by looping through the prev Tx set and calculating.
   uint64_t totIn = 0;
   for (const auto& inAddr : txDet[0].inputAddresses) {
      totIn += inAddr.value;
   }

   uint64_t fees = totIn - curTx_.getSumOfOutputs();
   float feePerByte = (float)fees / (float)curTx_.getTxWeight();
   ui_->tranInput->setText(UiUtils::displayAmount(totIn));
   ui_->tranFees->setText(UiUtils::displayAmount(fees));
   ui_->tranFeePerByte->setText(QString::number(nearbyint(feePerByte)));
   ui_->tranNumInputs->setText(QString::number(curTx_.getNumTxIn()));
   ui_->tranNumOutputs->setText(QString::number(curTx_.getNumTxOut()));
   ui_->tranOutput->setText(UiUtils::displayAmount(curTx_.getSumOfOutputs()));
   ui_->tranSize->setText(QString::number(curTx_.getTxWeight()));

   ui_->treeInput->clear();
   ui_->treeOutput->clear();

   std::map<BinaryData, unsigned int> hashCounts;
   for (const auto &inAddr : txDet[0].inputAddresses) {
      hashCounts[inAddr.outHash]++;
   }

   // here's the code to add data to the Input tree.
   for (const auto& inAddr : txDet[0].inputAddresses) {
      QString addrStr;
      const QString walletName = QString::fromStdString(inAddr.walletName);

      // For now, don't display any data if the TxOut is non-std. Displaying a
      // hex version of the script is one thing that could be done. This needs
      // to be discussed before implementing. Non-std could mean many things.
      if (inAddr.type == TXOUT_SCRIPT_NONSTANDARD) {
         addrStr = tr("<Non-Standard>");
      }
      else {
         addrStr = QString::fromStdString(inAddr.address.display());
      }

      // create a top level item using type, address, amount, wallet values
      addItem(ui_->treeInput, addrStr, inAddr.value, walletName, inAddr.outHash
         , (hashCounts[inAddr.outHash] > 1) ? inAddr.outIndex : -1);
   }

   std::vector<bs::sync::AddressDetails> outputAddresses = txDet[0].outputAddresses;
   if (!txDet[0].changeAddress.address.empty()) {
      outputAddresses.push_back(txDet[0].changeAddress);
   }
   for (const auto &outAddr : outputAddresses) {
      QString addrStr;
      QString walletName;

      // For now, don't display any data if the TxOut is OP_RETURN or non-std.
      // Displaying a hex version of the script is one thing that could be done.
      // This needs to be discussed before implementing. OP_RETURN isn't too bad
      // (80 bytes max) but non-std could mean just about anything.
      if (outAddr.type == TXOUT_SCRIPT_OPRETURN) {
         addrStr = tr("<OP_RETURN>");
      }
      else if (outAddr.type == TXOUT_SCRIPT_NONSTANDARD) {
         addrStr = tr("<Non-Standard>");
      }
      else {
         walletName = QString::fromStdString(outAddr.walletName);
         addrStr = QString::fromStdString(outAddr.address.display());
      }

      addItem(ui_->treeOutput, addrStr, outAddr.value, walletName, outAddr.outHash);
   }

   ui_->treeInput->resizeColumns();
   ui_->treeOutput->resizeColumns();
}

// Used in callback to process the Tx object returned by Armory.
void TransactionDetailsWidget::processTxData(const Tx &tx)
{
   // Save Tx and the prev Tx entries (get input amounts & such)
   curTx_ = tx;
   ui_->tranID->setText(QString::fromStdString(curTx_.getThisHash().toHexStr(true)));

   const auto txHeight = curTx_.getTxHeight();
   ui_->nbConf->setVisible(txHeight != UINT32_MAX);
   ui_->labelNbConf->setVisible(txHeight != UINT32_MAX);
   ui_->nbConf->setText(QString::number(armoryPtr_->getConfirmationsNumber(txHeight)));

   // Get each Tx object associated with the Tx's TxIn object. Needed to calc
   // the fees.
   const auto &cbProcessTX = [this]
      (const AsyncClient::TxBatchResult &prevTxs, std::exception_ptr)
   {
      prevTxMap_.insert(prevTxs.cbegin(), prevTxs.cend());
      // We're ready to display all the transaction-related data in the UI.
      setTxGUIValues();
   };

   std::set<BinaryData> prevTxHashSet; // A Tx's associated prev Tx hashes.
   // While here, we need to get the prev Tx with the UTXO being spent.
   // This is done so that we can calculate fees later.
   for (int i = 0; i < tx.getNumTxIn(); i++) {
      TxIn in = tx.getTxInCopy(i);
      OutPoint op = in.getOutPoint();
      const TxHash intPrevTXID(op.getTxHash());
      const auto &itTX = prevTxMap_.find(intPrevTXID);
      if (itTX == prevTxMap_.end()) {
         prevTxHashSet.insert(intPrevTXID);
      }
   }

   // Get the TxIn-associated Tx objects from Armory.
   if (prevTxHashSet.empty()) {
      setTxGUIValues();
   }
   else {
      armoryPtr_->getTXsByHash(prevTxHashSet, cbProcessTX, false);
   }
}

// The function that will actually populate the GUI with TX data.
void TransactionDetailsWidget::setTxGUIValues()
{
   // Get fees & fee/byte by looping through the prev Tx set and calculating.
   uint64_t totIn = 0;
   for (int i = 0; i < curTx_.getNumTxIn(); ++i) {
      const TxIn &in = curTx_.getTxInCopy(i);
      const OutPoint &op = in.getOutPoint();
      const auto &prevTx = prevTxMap_[op.getTxHash()];
      if (prevTx && prevTx->isInitialized()) {
         TxOut prevOut = prevTx->getTxOutCopy(op.getTxOutIndex());
         totIn += prevOut.getValue();
      }
   }

   emit finished();

   uint64_t fees = totIn - curTx_.getSumOfOutputs();
   float feePerByte = (float)fees / (float)curTx_.getTxWeight();

   // NB: Certain data (timestamp, height, and # of confs) can't be obtained
   // from the Tx object. For now, we're leaving placeholders until a solution
   // can be found. In theory, the timestamp can be obtained from the header.
   // The header data retrieved right now seems to be inaccurate, so we're not
   // using that right now.

   // Populate the GUI fields. (NOTE: Armory's getTxWeight() call needs to be
   // relabeled getVirtSize().)
   // Output TXID in RPC byte order by flipping TXID bytes rcv'd by Armory (internal
   // order).

   ui_->tranNumInputs->setText(QString::number(curTx_.getNumTxIn()));
   ui_->tranNumOutputs->setText(QString::number(curTx_.getNumTxOut()));
   ui_->tranInput->setText(UiUtils::displayAmount(totIn));
   ui_->tranOutput->setText(UiUtils::displayAmount(curTx_.getSumOfOutputs()));
   ui_->tranFees->setText(UiUtils::displayAmount(fees));
   ui_->tranFeePerByte->setText(QString::number(nearbyint(feePerByte)));
   ui_->tranSize->setText(QString::number(curTx_.getTxWeight()));

   loadInputs();
}

void TransactionDetailsWidget::onNewBlock(unsigned int curBlock)
{
   if (!armoryPtr_) {
      topBlock_ = curBlock;
      onTXDetails({});
      return;
   }
   if (curTx_.isInitialized()) {
      populateTransactionWidget(curTx_.getThisHash(), false);
   }
}

// Load the input and output windows.
void TransactionDetailsWidget::loadInputs()
{
   loadTreeIn(ui_->treeInput);
   loadTreeOut(ui_->treeOutput);
}

// Input widget population.
void TransactionDetailsWidget::loadTreeIn(CustomTreeWidget *tree)
{
   tree->clear();

   std::map<TxHash, unsigned int> hashCounts;
   for (int i = 0; i < curTx_.getNumTxIn(); i++) {
      TxOut prevOut;
      const OutPoint op = curTx_.getTxInCopy(i).getOutPoint();
      hashCounts[op.getTxHash()]++;
   }

   // here's the code to add data to the Input tree.
   for (int i = 0; i < curTx_.getNumTxIn(); i++) {
      TxOut prevOut;
      const TxIn in = curTx_.getTxInCopy(i);
      const OutPoint op = in.getOutPoint();
      const TxHash intPrevTXID(op.getTxHash());
      const auto &prevTx = prevTxMap_[intPrevTXID];
      if (prevTx && prevTx->isInitialized()) {
         prevOut = prevTx->getTxOutCopy(op.getTxOutIndex());
      }
      auto txType = prevOut.getScriptType();
      const auto outAddr = bs::Address::fromTxOut(prevOut);
      const auto addressWallet = walletsMgr_->getWalletByAddress(outAddr);
      QString addrStr;
      const QString walletName = addressWallet ? QString::fromStdString(addressWallet->name()) : QString();

      // For now, don't display any data if the TxOut is non-std. Displaying a
      // hex version of the script is one thing that could be done. This needs
      // to be discussed before implementing. Non-std could mean many things.
      if (txType == TXOUT_SCRIPT_NONSTANDARD) {
         addrStr = tr("<Non-Standard>");
      }
      else {
         addrStr = QString::fromStdString(outAddr.display());
      }

      // create a top level item using type, address, amount, wallet values
      addItem(tree, addrStr, prevOut.getValue(), walletName, intPrevTXID
         , (hashCounts[intPrevTXID] > 1) ? op.getTxOutIndex() : -1);
   }
   tree->resizeColumns();
}

// Output widget population.
void TransactionDetailsWidget::loadTreeOut(CustomTreeWidget *tree)
{
   tree->clear();

   // here's the code to add data to the Input tree.
   for (int i = 0; i < curTx_.getNumTxOut(); i++) {
      TxOut txOut = curTx_.getTxOutCopy(i);
      auto txType = txOut.getScriptType();
      QString addrStr;
      QString walletName;

      // For now, don't display any data if the TxOut is OP_RETURN or non-std.
      // Displaying a hex version of the script is one thing that could be done.
      // This needs to be discussed before implementing. OP_RETURN isn't too bad
      // (80 bytes max) but non-std could mean just about anything.
      if (txType == TXOUT_SCRIPT_OPRETURN) {
         addrStr = tr("<OP_RETURN>");
      } else if (txType == TXOUT_SCRIPT_NONSTANDARD) {
         addrStr = tr("<Non-Standard>");
      } else {
         const auto outAddr = bs::Address::fromTxOut(txOut);
         const auto addressWallet = walletsMgr_->getWalletByAddress(outAddr);
         walletName = addressWallet ? QString::fromStdString(addressWallet->name()) : QString();
         addrStr = QString::fromStdString(outAddr.display());
      }

      addItem(tree, addrStr, txOut.getValue(), walletName, txOut.getScript());

      // add the item to the tree
   }
   tree->resizeColumns();
}

void TransactionDetailsWidget::addItem(QTreeWidget *tree, const QString &address
   , const uint64_t amount, const QString &wallet, const BinaryData &txHash
   , const int txIndex)
{
   const bool specialAddr = address.startsWith(QLatin1Char('<'));
   const bool isOutput = (tree == ui_->treeOutput);
   QTreeWidgetItem *item = nullptr;
   for (int i = 0; i < tree->topLevelItemCount(); ++i) {
      const auto tlItem = tree->topLevelItem(i);
      if (tlItem->data(0, Qt::DisplayRole).toString() == address) {
         item = tlItem;
         break;
      }
   }
   if (!item || specialAddr) {
      QStringList items;
      const auto amountStr = UiUtils::displayAmount(amount);
      items << address << amountStr << wallet;
      item = new QTreeWidgetItem(items);
      item->setData(0, Qt::UserRole, isOutput);
      item->setData(1, Qt::UserRole, (qulonglong)amount);
      tree->addTopLevelItem(item);
      item->setExpanded(true);
   }
   else {
      const auto itemData = item->data(1, Qt::UserRole);
      if (!itemData.isValid() || itemData.isNull()) {
         return;
      }
      uint64_t prevAmount = itemData.toULongLong();
      prevAmount += amount;
      item->setData(1, Qt::UserRole, (qulonglong)prevAmount);
      item->setData(1, Qt::DisplayRole, UiUtils::displayAmount(prevAmount));
   }
   if (!specialAddr) {
      auto txHashStr = QString::fromStdString(txHash.toHexStr(!isOutput));
      if (txIndex >= 0) {
         txHashStr += QLatin1String(":") + QString::number(txIndex);
      }
      QStringList txItems;
      txItems << txHashStr << UiUtils::displayAmount(amount);
      auto txHashItem = new QTreeWidgetItem(txItems);
      if (!isOutput) {
         txHashItem->setData(0, Qt::UserRole, QString::fromStdString(txHash.toHexStr(true)));
      }
      txHashItem->setData(1, Qt::UserRole, (qulonglong)amount);
      if (item) {
         item->addChild(txHashItem);
      }
   }
}

// A function that sends a signal to the explorer widget to open the address
// details widget for a clicked address. Doesn't apply to OP_RETURN or non-std
// addresses.
void TransactionDetailsWidget::onAddressClicked(QTreeWidgetItem *item, int column)
{
   if (item->childCount() > 0) {
      emit addressClicked(item->text(colAddressId));
   }
   else {
      const auto txHashStr = item->data(colAddressId, Qt::UserRole).toString();
      if (!txHashStr.isEmpty()) {
         emit txHashClicked(txHashStr);
      }
   }
}

// Clear all the fields.
void TransactionDetailsWidget::clear()
{
   prevTxMap_.clear();
   curTx_ = Tx();
   curTxHash_.clear();

   ui_->tranID->clear();
   ui_->tranNumInputs->clear();
   ui_->tranNumOutputs->clear();
   ui_->tranInput->clear();
   ui_->tranOutput->clear();
   ui_->tranFees->clear();
   ui_->tranFeePerByte->clear();
   ui_->tranSize->clear();
   ui_->treeInput->clear();
   ui_->treeOutput->clear();
}
