#include "CreateTransactionDialogAdvanced.h"

#include "ui_CreateTransactionDialogAdvanced.h"

#include "Address.h"
#include "CoinControlDialog.h"
#include "FixedFeeValidator.h"
#include "MessageBoxInfo.h"
#include "OfflineSigner.h"
#include "PyBlockDataManager.h"
#include "SelectAddressDialog.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "TransactionOutputsModel.h"
#include "UiUtils.h"
#include "UsedInputsModel.h"
#include "WalletsManager.h"
#include "XbtAmountValidator.h"

#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QFile>
#include <QFileDialog>
#include <QPushButton>
#include <QIntValidator>

#include <stdexcept>


CreateTransactionDialogAdvanced::CreateTransactionDialogAdvanced(const std::shared_ptr<WalletsManager>& walletManager
   , const std::shared_ptr<SignContainer> &container, bool loadFeeSuggestions, QWidget* parent)
 : CreateTransactionDialog(walletManager, container, loadFeeSuggestions, parent)
 , ui_(new Ui::CreateTransactionDialogAdvanced)
{
   ui_->setupUi(this);

   selectedChangeAddress_ = bs::Address{};

   initUI();
}

std::shared_ptr<CreateTransactionDialogAdvanced> CreateTransactionDialogAdvanced::CreateForRBF(
        const std::shared_ptr<WalletsManager>& walletManager
      , const std::shared_ptr<SignContainer>& container
      , const Tx&  tx
      , const std::shared_ptr<bs::Wallet>& wallet
      , QWidget* parent)
{
   auto dlg = std::make_shared<CreateTransactionDialogAdvanced>(walletManager, container, true, parent);

   dlg->setWindowTitle(tr("Replace-By-Fee"));
   dlg->SetFixedWallet(wallet->GetWalletId());

   dlg->ui_->checkBoxRBF->setChecked(true);
   dlg->ui_->checkBoxRBF->setEnabled(false);
   dlg->ui_->pushButtonImport->setEnabled(false);

   // set inputs
   auto selInputs = dlg->transactionData_->GetSelectedInputs();
   selInputs->SetUseAutoSel(false);

   auto rbfList = wallet->getRBFTxOutList();
   selInputs->SetInputs(rbfList, {});

   const auto &bdm = PyBlockDataManager::instance();
   int64_t totalVal = 0;

   for (int i = 0; i < tx.getNumTxIn(); i++) {
      const auto txin = tx.getTxInCopy(i);
      const auto outpoint = txin.getOutPoint();
      Tx prevTx = bdm->getTxByHash(outpoint.getTxHash());
      if (prevTx.isInitialized()) {
         TxOut prevOut = prevTx.getTxOutCopy(outpoint.getTxOutIndex());
         totalVal += prevOut.getValue();
      }
      if (!selInputs->SetUTXOSelection(outpoint.getTxHash(), outpoint.getTxOutIndex())) {
         throw std::runtime_error("No input[s] found");
      }
   }

   QString  changeAddress;
   double   changeAmount;

   // set outputs
   for (int i = 0; i < tx.getNumTxOut(); i++) {
      TxOut out = tx.getTxOutCopy(i);
      const auto addr = bs::Address::fromTxOut(out);

      const auto addressString = addr.display();
      const auto amount = UiUtils::amountToBtc(out.getValue());

      // use last output as change addres
      if (wallet->containsAddress(addr)) {
         if (!changeAddress.isEmpty()) {
            dlg->AddRecipient(changeAddress, changeAmount);
         }

         changeAddress = addressString;
         changeAmount = amount;
      } else {
         dlg->AddRecipient(addressString, amount);
      }

      totalVal -= out.getValue();
   }

   dlg->SetFixedChangeAddress(changeAddress);

   // set fee
   if (totalVal < 0) {
      throw std::runtime_error("Negative amount");
   }

   dlg->originalFee_ = totalVal;
   const auto &txSize = tx.serializeNoWitness().getSize();
   const float feePerByte = std::ceil((float)totalVal / txSize);
   dlg->SetMinimumFee(totalVal, feePerByte + dlg->minRelayFeePerByte_);

   dlg->disableInputSelection();
   dlg->onTransactionUpdated();

   if (changeAddress.isNull()) {
      dlg->setUnchangeableTx();
   }

   return dlg;
}

std::shared_ptr<CreateTransactionDialogAdvanced> CreateTransactionDialogAdvanced::CreateForCPFP(
        const std::shared_ptr<WalletsManager>& walletManager
      , const std::shared_ptr<SignContainer>& container
      , const std::shared_ptr<bs::Wallet>& wallet
      , const Tx &tx
      , QWidget* parent)
{
   auto dlg = std::make_shared<CreateTransactionDialogAdvanced>(walletManager, container, true, parent);

   dlg->setWindowTitle(tr("Child-Pays-For-Parent"));
   dlg->SetFixedWallet(wallet->GetWalletId());
   dlg->ui_->pushButtonImport->setEnabled(false);

   // set inputs
   auto selInputs = dlg->transactionData_->GetSelectedInputs();
   selInputs->SetUseAutoSel(false);

   int64_t totalVal = 0;
   const auto &bdm = PyBlockDataManager::instance();

   for (int i = 0; i < tx.getNumTxIn(); i++) {
      const auto txin = tx.getTxInCopy(i);
      const auto outpoint = txin.getOutPoint();
      Tx prevTx = bdm->getTxByHash(outpoint.getTxHash());
      if (prevTx.isInitialized()) {
         TxOut prevOut = prevTx.getTxOutCopy(outpoint.getTxOutIndex());
         totalVal += prevOut.getValue();
      }
   }

   unsigned int cntOutputs = 0;
   for (int i = 0; i < tx.getNumTxOut(); i++) {
      auto out = tx.getTxOutCopy(i);
      const auto addr = bs::Address::fromTxOut(out);
      if (wallet->containsAddress(addr)) {
         if (selInputs->SetUTXOSelection(out.getParentHash(), out.getIndex())) {
            cntOutputs++;
         }
      }
      totalVal -= out.getValue();
   }

   if (!cntOutputs) {
      throw std::runtime_error("No input[s] found");
   }
   if (totalVal < 0) {
      throw std::runtime_error("negative TX balance");
   }

   const auto fee2Blocks = walletManager->estimatedFeePerByte(2);
   const auto txSize = tx.serializeNoWitness().getSize();
   const float feePerByte = (float)totalVal / txSize;
   dlg->originalFee_ = totalVal;
   const size_t projectedTxSize = 85;  // 1 input and 1 output bech32
   float totalFee = std::abs(txSize * (fee2Blocks - feePerByte) + projectedTxSize * fee2Blocks);
   dlg->SetMinimumFee(totalFee, std::ceil(totalFee / (txSize + projectedTxSize)));

   dlg->onTransactionUpdated();
   return dlg;
}

void CreateTransactionDialogAdvanced::initUI()
{
   usedInputsModel_ = new UsedInputsModel(this);
   outputsModel_ = new TransactionOutputsModel(this);

   CreateTransactionDialog::init();

   ui_->treeViewInputs->setModel(usedInputsModel_);
   ui_->treeViewInputs->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   ui_->treeViewOutputs->setModel(outputsModel_);
   ui_->treeViewOutputs->setColumnWidth(2, 30);
   ui_->treeViewOutputs->header()->setSectionResizeMode(2, QHeaderView::Fixed);
   ui_->treeViewOutputs->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
   ui_->treeViewOutputs->header()->setSectionResizeMode(0, QHeaderView::Stretch);

   connect(outputsModel_, &TransactionOutputsModel::rowsInserted, [this](const QModelIndex &parent, int first, int last)
   {
      for (int i = first; i <= last; i++) {
         auto index = outputsModel_->index(i, 2);
         auto outputId = outputsModel_->GetOutputId(i);

         auto button = new QPushButton();
         button->setFixedSize(30, 16);
         button->setContentsMargins(0, 0, 0, 0);

         button->setIcon(UiUtils::icon(0xeaf1, QVariantMap{
            { QLatin1String{ "color" }, QColor{ Qt::white } }
         }));

         ui_->treeViewOutputs->setIndexWidget(index, button);

         connect(button, &QPushButton::clicked, [this, outputId]()
            {
               RemoveOutputByRow(outputsModel_->GetRowById(outputId));
            });
      }
   });

   currentAddressValid_ = false;
   currentValue_ = 0;

   ui_->pushButtonAddOutput->setEnabled(false);
   ui_->line->hide();

   connect(ui_->comboBoxWallets, SIGNAL(currentIndexChanged(int)), this, SLOT(selectedWalletChanged(int)));

   connect(ui_->lineEditAddress, &QLineEdit::textChanged, this, &CreateTransactionDialogAdvanced::onAddressTextChanged);
   connect(ui_->lineEditAmount, &QLineEdit::textChanged, this, &CreateTransactionDialogAdvanced::onXBTAmountChanged);
   ui_->lineEditAddress->installEventFilter(this);
   ui_->lineEditAmount->installEventFilter(this);

   connect(ui_->pushButtonSelectInputs, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onSelectInputs);
   connect(ui_->pushButtonAddOutput, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onAddOutput);
   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onCreatePressed);
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onImportPressed);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::reject);

   ui_->radioButtonNewAddrNative->setChecked(true);

   connect(ui_->radioButtonNewAddrNative, &QRadioButton::clicked, this, &CreateTransactionDialogAdvanced::onNewAddressSelectedForChange);
   connect(ui_->radioButtonNewAddrNested, &QRadioButton::clicked, this, &CreateTransactionDialogAdvanced::onNewAddressSelectedForChange);
   connect(ui_->radioButtonExistingAddress, &QRadioButton::clicked, this, &CreateTransactionDialogAdvanced::onExistingAddressSelectedForChange);

   ui_->treeViewOutputs->setContextMenuPolicy(Qt::CustomContextMenu);

   removeOutputAction_ = new QAction(tr("Remove Output"), this);
   connect(removeOutputAction_, &QAction::triggered, this, &CreateTransactionDialogAdvanced::onRemoveOutput);
   contextMenu_.addAction(removeOutputAction_);

   connect(ui_->treeViewOutputs, &QTreeView::customContextMenuRequested, this, &CreateTransactionDialogAdvanced::showContextMenu);
}

void CreateTransactionDialogAdvanced::clear()
{
   CreateTransactionDialog::clear();

   outputsModel_->clear();
   usedInputsModel_->clear();
}

bool CreateTransactionDialogAdvanced::eventFilter(QObject *watched, QEvent *evt)
{
   if (evt->type() == QEvent::KeyPress) {
      auto keyID = static_cast<QKeyEvent *>(evt)->key();
      if ((keyID == Qt::Key_Return) || (keyID == Qt::Key_Enter)) {
         if ((watched == ui_->lineEditAddress) && currentAddressValid_ && qFuzzyIsNull(currentValue_)) {
            ui_->lineEditAmount->setFocus();
         }
         if ((watched == ui_->lineEditAmount) && !qFuzzyIsNull(currentValue_) && !currentAddressValid_) {
            ui_->lineEditAddress->setFocus();
         }
         else if (ui_->pushButtonAddOutput->isEnabled()) {
            ui_->pushButtonAddOutput->animateClick();
            ui_->lineEditAddress->setFocus();
         }
         else if (ui_->lineEditAddress->text().isEmpty() && ui_->lineEditAmount->text().isEmpty()) {
            return QDialog::eventFilter(watched, evt);
         }
         return true;
      }
   }
   return QDialog::eventFilter(watched, evt);
}

QComboBox *CreateTransactionDialogAdvanced::comboBoxWallets() const
{
   return ui_->comboBoxWallets;
}

QComboBox *CreateTransactionDialogAdvanced::comboBoxFeeSuggestions() const
{
   return ui_->comboBoxFeeSuggestions;
}

QLineEdit *CreateTransactionDialogAdvanced::lineEditAddress() const
{
   return ui_->lineEditAddress;
}

QLineEdit *CreateTransactionDialogAdvanced::lineEditAmount() const
{
   return ui_->lineEditAmount;
}

QPushButton *CreateTransactionDialogAdvanced::pushButtonMax() const
{
   return ui_->pushButtonMax;
}

QTextEdit *CreateTransactionDialogAdvanced::textEditComment() const
{
   return ui_->textEditComment;
}

QCheckBox *CreateTransactionDialogAdvanced::checkBoxRBF() const
{
   return ui_->checkBoxRBF;
}

QLabel *CreateTransactionDialogAdvanced::labelBalance() const
{
   return ui_->labelBalance;
}

QLabel *CreateTransactionDialogAdvanced::labelAmount() const
{
   return ui_->labelInputAmount;
}

QLabel *CreateTransactionDialogAdvanced::labelTxInputs() const
{
   return ui_->labelTXInputs;
}

QLabel *CreateTransactionDialogAdvanced::labelEstimatedFee() const
{
   return ui_->labelFee;
}

QLabel *CreateTransactionDialogAdvanced::labelTotalAmount() const
{
   return ui_->labelTransationAmount;
}

QLabel *CreateTransactionDialogAdvanced::labelTxSize() const
{
   return ui_->labelTxSize;
}

QPushButton *CreateTransactionDialogAdvanced::pushButtonCreate() const
{
   return ui_->pushButtonCreate;
}

QPushButton *CreateTransactionDialogAdvanced::pushButtonCancel() const
{
   return ui_->pushButtonCancel;
}

QLabel* CreateTransactionDialogAdvanced::feePerByteLabel() const
{
   return ui_->labelFeePerByte;
}

QLabel* CreateTransactionDialogAdvanced::changeLabel() const
{
   return ui_->labelReturnAmount;
}

void CreateTransactionDialogAdvanced::showContextMenu(const QPoint &point)
{
   if (!removeOutputEnabled_) {
      return;
   }

   const auto index = ui_->treeViewOutputs->indexAt(point);
   if (index.row() != -1) {
      removeOutputAction_->setEnabled(true);
      removeOutputAction_->setData(index.row());

      contextMenu_.exec(ui_->treeViewOutputs->mapToGlobal(point));
   }
}

void CreateTransactionDialogAdvanced::onRemoveOutput()
{
   int row = removeOutputAction_->data().toInt();
   RemoveOutputByRow(row);
}

void CreateTransactionDialogAdvanced::RemoveOutputByRow(int row)
{
   auto outputId = outputsModel_->GetOutputId(row);

   transactionData_->RemoveRecipient(outputId);
   outputsModel_->RemoveRecipient(row);

   ui_->comboBoxFeeSuggestions->setEnabled(true);
}

void CreateTransactionDialogAdvanced::selectedWalletChanged(int index)
{
   CreateTransactionDialog::selectedWalletChanged(index);

   ui_->radioButtonNewAddrNative->setChecked(true);
}

void CreateTransactionDialogAdvanced::onTransactionUpdated()
{
   CreateTransactionDialog::onTransactionUpdated();

   usedInputsModel_->updateInputs(transactionData_->inputs());

   const auto &summary = transactionData_->GetTransactionSummary();

   if (!changeAddressFixed_) {
      bool changeSelectionEnabled = summary.hasChange || (summary.transactionSize == 0);
      ui_->changeAddrGroupBox->setEnabled(changeSelectionEnabled);
      showExistingChangeAddress(changeSelectionEnabled);
   }

   if (originalFee_) {
      SetMinimumFee(originalFee_ + minRelayFeePerByte_ * summary.transactionSize, minFeePerByte_);
   }
   validateCreateButton();
}

void CreateTransactionDialogAdvanced::preSetAddress(const QString& address)
{
   ui_->lineEditAddress->setText(address);
   onAddressTextChanged(address);
}

void CreateTransactionDialogAdvanced::preSetValue(const double value)
{
   ui_->lineEditAmount->setText(UiUtils::displayAmount(value));
}

void CreateTransactionDialogAdvanced::onAddressTextChanged(const QString& addressString)
{
   try {
      bs::Address address{addressString.trimmed()};
      currentAddressValid_ = address.isValid();
   } catch (...) {
      currentAddressValid_ = false;
   }

   if (currentAddressValid_)
      UiUtils::setWrongState(ui_->lineEditAddress, false);
   else
      UiUtils::setWrongState(ui_->lineEditAddress, true);

   validateAddOutputButton();
}

void CreateTransactionDialogAdvanced::onXBTAmountChanged(const QString &text)
{
   currentValue_ = UiUtils::parseAmountBtc(text);
   validateAddOutputButton();
}

void CreateTransactionDialogAdvanced::onSelectInputs()
{
   CoinControlDialog dlg(transactionData_->GetSelectedInputs(), this);
   dlg.exec();
}

void CreateTransactionDialogAdvanced::onAddOutput()
{
   const bs::Address address(ui_->lineEditAddress->text().trimmed());

   auto maxValue = transactionData_->CalculateMaxAmount(address);
   const bool maxAmount = qFuzzyCompare(maxValue, currentValue_);

   AddRecipient(address, currentValue_, maxAmount);

   // clear edits
   ui_->lineEditAddress->clear();
   ui_->lineEditAmount->clear();
   if (maxAmount) {
      ui_->comboBoxFeeSuggestions->setEnabled(false);
   }

   ui_->pushButtonAddOutput->setEnabled(false);
}

void CreateTransactionDialogAdvanced::AddRecipient(const bs::Address &address, double amount, bool isMax)
{
   auto recipientId = transactionData_->RegisterNewRecipient();

   transactionData_->UpdateRecipientAddress(recipientId, address);
   transactionData_->UpdateRecipientAmount(recipientId, amount);

   // add to the model
   outputsModel_->AddRecipient(recipientId, address.display(), amount);
}

void CreateTransactionDialogAdvanced::validateAddOutputButton()
{
   ui_->pushButtonMax->setEnabled(currentAddressValid_);
   ui_->pushButtonAddOutput->setEnabled(currentAddressValid_
                                        && !qFuzzyIsNull(currentValue_));
}

void CreateTransactionDialogAdvanced::validateCreateButton()
{
   const bool isSignerReady = signingContainer_ && ((signingContainer_->opMode() == SignContainer::OpMode::Offline)
      || !signingContainer_->isOffline());
   const bool isTxValid = transactionData_->IsTransactionValid() && transactionData_->GetTransactionSummary().transactionSize;

   ui_->pushButtonCreate->setEnabled(isTxValid
      && isSignerReady
      && !broadcasting_
      && (ui_->radioButtonNewAddrNative->isChecked() || ui_->radioButtonNewAddrNested->isChecked()
         || (selectedChangeAddress_.isValid())));
}

void CreateTransactionDialogAdvanced::AddManualFeeEntries(float feePerByte, float totalFee)
{
   ui_->comboBoxFeeSuggestions->addItem(tr("Manual Fee Selection"), feePerByte);
   ui_->comboBoxFeeSuggestions->addItem(tr("Total Network Fee"), totalFee);
}

void CreateTransactionDialogAdvanced::onFeeSuggestionsLoaded(const std::map<unsigned int, float> &feeValues)
{
   if (feeChangeDisabled_) {
      return;
   }
   
   CreateTransactionDialog::onFeeSuggestionsLoaded(feeValues);

   AddManualFeeEntries((minFeePerByte_ > 0) ? minFeePerByte_ : feeValues.begin()->second
      , (minTotalFee_ > 0) ? minTotalFee_ : 0);

   if (minFeePerByte_ > 0) {
      const auto index = ui_->comboBoxFeeSuggestions->count() - 2;
      ui_->comboBoxFeeSuggestions->setCurrentIndex(index);
      feeSelectionChanged(index);
   }
}

void CreateTransactionDialogAdvanced::onManualFeeChanged(int fee)
{
   if (ui_->comboBoxFeeSuggestions->currentIndex() == (ui_->comboBoxFeeSuggestions->count() - 2)) {
      transactionData_->SetFeePerByte(fee);
   }
   else {
      transactionData_->SetTotalFee(fee);
   }
}

void CreateTransactionDialogAdvanced::SetMinimumFee(float totalFee, float feePerByte)
{
   minTotalFee_ = totalFee;
   minFeePerByte_ = feePerByte;

   if (loadFeeSuggestions_ && (ui_->comboBoxFeeSuggestions->count() >= 2)) {
      ui_->comboBoxFeeSuggestions->setItemData(ui_->comboBoxFeeSuggestions->count() - 2, feePerByte);
      ui_->comboBoxFeeSuggestions->setItemData(ui_->comboBoxFeeSuggestions->count() - 1, totalFee);
   }
}

void CreateTransactionDialogAdvanced::feeSelectionChanged(int currentIndex)
{
   if (currentIndex < (ui_->comboBoxFeeSuggestions->count() - 2)) {
      CreateTransactionDialog::feeSelectionChanged(currentIndex);

      ui_->comboBoxFeeSuggestions->setEditable(false);
   } else {
      const auto &feeVal = ui_->comboBoxFeeSuggestions->currentData().toFloat();
      if (currentIndex == (ui_->comboBoxFeeSuggestions->count() - 2)) {
         setFixedFee(feeVal, true);
      }
      else {
         setFixedFee(feeVal, false);
      }
   }
}

bs::Address CreateTransactionDialogAdvanced::getChangeAddress() const
{
   bs::Address result;
   if (transactionData_->GetTransactionSummary().hasChange) {
      if (ui_->radioButtonNewAddrNative->isChecked() || ui_->radioButtonNewAddrNested->isChecked()) {
         result = transactionData_->GetWallet()->GetNewChangeAddress(
            ui_->radioButtonNewAddrNative->isChecked() ? AddressEntryType_P2WPKH : AddressEntryType_P2SH);
         transactionData_->createAddress(result);
         transactionData_->GetWallet()->SetAddressComment(result, bs::wallet::Comment::toString(bs::wallet::Comment::ChangeAddress));
      } else {
         result = selectedChangeAddress_;
      }
   }
   return result;
}

void CreateTransactionDialogAdvanced::onCreatePressed()
{
   if (!importedSignedTX_.isNull()) {
      if (BroadcastImportedTx()) {
         accept();
      }
      else {
         initUI();
         validateCreateButton();
      }
      return;
   }

   if (!CreateTransaction()) {
      reject();
   }
}

bool CreateTransactionDialogAdvanced::HaveSignedImportedTransaction() const
{
   return !importedSignedTX_.isNull();
}

void CreateTransactionDialogAdvanced::SetImportedTransactions(const std::vector<bs::wallet::TXSignRequest>& transactions)
{
   ui_->pushButtonCreate->setEnabled(false);
   ui_->pushButtonCreate->setText(tr("Broadcast"));

   const auto &tx = transactions[0];
   if (!tx.prevStates.empty()) {    // signed TX
      ui_->textEditComment->insertPlainText(QString::fromStdString(tx.comment));

      if (tx.prevStates.size() == 1) {
         importedSignedTX_ = tx.prevStates[0];

         Tx tx(importedSignedTX_);
         if (tx.isInitialized()) {
            ui_->pushButtonCreate->setEnabled(true);

            std::shared_ptr<bs::Wallet> wallet;
            std::vector<std::pair<BinaryData, uint32_t>> utxoHashes;
            const auto &bdm = PyBlockDataManager::instance();
            int64_t totalVal = 0;
            for (size_t i = 0; i < tx.getNumTxIn(); i++) {
               auto in = tx.getTxInCopy((int)i);
               OutPoint op = in.getOutPoint();
               utxoHashes.push_back({ op.getTxHash(), op.getTxOutIndex() });
               Tx prevTx = bdm->getTxByHash(op.getTxHash());
               if (prevTx.isInitialized()) {
                  const auto prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
                  totalVal += prevOut.getValue();
                  if (!wallet) {
                     const auto addr = bs::Address::fromTxOut(prevOut);
                     const auto &addrWallet = walletsManager_->GetWalletByAddress(addr);
                     if (addrWallet) {
                        wallet = addrWallet;
                     }
                  }
               }
            }

            if (wallet) {
               SetFixedWallet(wallet->GetWalletId());
               auto selInputs = transactionData_->GetSelectedInputs();
               for (const auto &txHash : utxoHashes) {
                  selInputs->SetUTXOSelection(txHash.first, txHash.second);
               }
            }
            for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
               TxOut out = tx.getTxOutCopy((int)i);
               const auto addr = bs::Address::fromTxOut(out);
               if (wallet && (i == (tx.getNumTxOut() - 1)) && (wallet->containsAddress(addr))) {
                  SetFixedChangeAddress(addr.display());
               } else {
                  AddRecipient(addr.display(), out.getValue() / BTCNumericTypes::BalanceDivider);
               }
               totalVal -= out.getValue();
            }
            SetPredefinedFee(totalVal);
         }
      }
   } else {
      SetFixedWallet(tx.walletId);
      if (tx.change.value) {
         SetFixedChangeAddress(tx.change.address.display());
      }
      SetPredefinedFee(tx.fee);
      labelEstimatedFee()->setText(UiUtils::displayAmount(tx.fee));
      ui_->textEditComment->insertPlainText(QString::fromStdString(tx.comment));
      auto selInputs = transactionData_->GetSelectedInputs();
      for (const auto &utxo : tx.inputs) {
         selInputs->SetUTXOSelection(utxo.getTxHash(), utxo.getTxOutIndex());
      }

      for (const auto &recip : tx.recipients) {
         const auto addr = bs::Address::fromRecipient(recip);
         AddRecipient(addr.display(), recip->getValue() / BTCNumericTypes::BalanceDivider);
      }

      if (!signingContainer_->isOffline() && tx.isValid()) {
         ui_->pushButtonCreate->setEnabled(true);
      }
   }

   ui_->checkBoxRBF->setChecked(tx.RBF);
   ui_->checkBoxRBF->setEnabled(false);

   disableOutputsEditing();
   disableInputSelection();
   disableFeeChanging();
   updateCreateButtonText();
   disableChangeAddressSelecting();
}

void CreateTransactionDialogAdvanced::onImportPressed()
{
   const auto transactions = ImportTransactions();
   if (transactions.empty()) {
      return;
   }

   SetImportedTransactions(transactions);
}

void CreateTransactionDialogAdvanced::onNewAddressSelectedForChange()
{
   selectedChangeAddress_ = bs::Address{};
   showExistingChangeAddress(false);
}

void CreateTransactionDialogAdvanced::onExistingAddressSelectedForChange()
{
   SelectAddressDialog selectAddressDialog(walletsManager_, transactionData_->GetWallet(), this
      , AddressListModel::AddressType::Internal);

   if (selectAddressDialog.exec() == QDialog::Accepted) {
      selectedChangeAddress_ = selectAddressDialog.getSelectedAddress();
      showExistingChangeAddress(true);
   } else {
      if (!selectedChangeAddress_.isValid()) {
         ui_->radioButtonNewAddrNative->setChecked(true);
      }
   }
}

void CreateTransactionDialogAdvanced::SetFixedWallet(const std::string& walletId)
{
   SelectWallet(walletId);
   selectedWalletChanged(0);
   ui_->comboBoxWallets->setEnabled(false);
}

void CreateTransactionDialogAdvanced::disableOutputsEditing()
{
   ui_->lineEditAddress->setEnabled(false);
   ui_->lineEditAmount->setEnabled(false);
   ui_->pushButtonMax->setEnabled(false);
   ui_->pushButtonAddOutput->setEnabled(false);
   ui_->treeViewOutputs->setEnabled(false);

   removeOutputEnabled_ = false;
}

void CreateTransactionDialogAdvanced::disableInputSelection()
{
   ui_->pushButtonSelectInputs->setEnabled(false);
}

void CreateTransactionDialogAdvanced::disableFeeChanging()
{
   feeChangeDisabled_ = true;
   ui_->comboBoxFeeSuggestions->setEditable(false);
   ui_->comboBoxFeeSuggestions->setEnabled(false);
}

void CreateTransactionDialogAdvanced::SetFixedChangeAddress(const QString& changeAddress)
{
   ui_->radioButtonExistingAddress->setChecked(true);

   ui_->radioButtonNewAddrNative->setEnabled(false);
   ui_->radioButtonNewAddrNested->setEnabled(false);

   selectedChangeAddress_ = bs::Address{changeAddress};
   showExistingChangeAddress(true);

   changeAddressFixed_ = true;
}

void CreateTransactionDialogAdvanced::SetPredefinedFee(const int64_t& manualFee)
{
   ui_->comboBoxFeeSuggestions->clear();
   ui_->comboBoxFeeSuggestions->addItem(tr("%1 satoshi").arg(manualFee), (qlonglong)manualFee);
   transactionData_->SetTotalFee(manualFee);
}

void CreateTransactionDialogAdvanced::setFixedFee(const int64_t& manualFee, bool perByte)
{
   ui_->comboBoxFeeSuggestions->setEditable(true);

   auto lineEdit = new QLineEdit(this);
   ui_->comboBoxFeeSuggestions->setLineEdit(lineEdit);

   auto feeValidator = new FixedFeeValidator(manualFee, perByte ? tr(" s/b") :  tr(" satoshi")
      , ui_->comboBoxFeeSuggestions);
   feeValidator->setMinValue(perByte ? minFeePerByte_ : minTotalFee_);
   connect(feeValidator, &FixedFeeValidator::feeUpdated, this, &CreateTransactionDialogAdvanced::onManualFeeChanged);

   if (perByte) {
      transactionData_->SetFeePerByte(manualFee);
   } else {
      transactionData_->SetTotalFee(manualFee);
   }

   ui_->comboBoxFeeSuggestions->setFocus();
}

void CreateTransactionDialogAdvanced::setUnchangeableTx()
{
   ui_->comboBoxFeeSuggestions->setEditable(false);
   ui_->comboBoxFeeSuggestions->setEnabled(false);
   ui_->treeViewOutputs->setEnabled(false);
   ui_->lineEditAddress->setEnabled(false);
   ui_->lineEditAmount->setEnabled(false);
   ui_->textEditComment->setEnabled(false);
   ui_->pushButtonCreate->setEnabled(false);
}

void CreateTransactionDialogAdvanced::showExistingChangeAddress(bool show)
{
   if (show && selectedChangeAddress_.isValid()) {
      ui_->labelChangeAddress->setText(selectedChangeAddress_.display());
   } else {
      ui_->labelChangeAddress->clear();
   }
}

void CreateTransactionDialogAdvanced::disableChangeAddressSelecting()
{
   ui_->widgetChangeAddress->setEnabled(false);
}
