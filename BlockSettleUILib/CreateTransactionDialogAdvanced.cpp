/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CreateTransactionDialogAdvanced.h"
#include "ui_CreateTransactionDialogAdvanced.h"

#include "Address.h"
#include "ArmoryConnection.h"
#include "BSMessageBox.h"
#include "CoinControlDialog.h"
#include "CreateTransactionDialogSimple.h"
#include "Wallets/HeadlessContainer.h"
#include "SelectAddressDialog.h"
#include "Wallets/SelectedTransactionInputs.h"
#include "Wallets/TransactionData.h"
#include "TransactionOutputsModel.h"
#include "UiUtils.h"
#include "UsedInputsModel.h"
#include "UtxoReservationManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "XbtAmountValidator.h"

#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>

#include <spdlog/spdlog.h>
#include <stdexcept>

static const float kDustFeePerByte = 3.0;


CreateTransactionDialogAdvanced::CreateTransactionDialogAdvanced(bool loadFeeSuggestions
   , uint32_t topBlock, const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<TransactionData>& txData, bs::UtxoReservationToken utxoReservation
   , QWidget* parent)
   : CreateTransactionDialog(loadFeeSuggestions, topBlock, logger, parent)
   , ui_(new Ui::CreateTransactionDialogAdvanced)
{
   transactionData_ = txData;
   selectedChangeAddress_ = bs::Address{};

   ui_->setupUi(this);
}

CreateTransactionDialogAdvanced::~CreateTransactionDialogAdvanced() = default;

std::shared_ptr<CreateTransactionDialogAdvanced> CreateTransactionDialogAdvanced::CreateForRBF(
      uint32_t topBlock, const std::shared_ptr<spdlog::logger>& logger
      , const Tx &tx, QWidget* parent)
{
   auto dlg = std::make_shared<CreateTransactionDialogAdvanced>(topBlock, false
      , logger, nullptr, bs::UtxoReservationToken(), parent);

   dlg->setWindowTitle(tr("Replace-By-Fee"));

   dlg->ui_->checkBoxRBF->setChecked(true);
   dlg->ui_->checkBoxRBF->setEnabled(false);

   dlg->ui_->pushButtonImport->setEnabled(false);
   dlg->ui_->pushButtonShowSimple->setEnabled(false);

   dlg->setRBFinputs(tx);
   dlg->isRBF_ = true;
   return dlg;
}

std::shared_ptr<CreateTransactionDialogAdvanced> CreateTransactionDialogAdvanced::CreateForCPFP(
        uint32_t topBlock, const std::string& walletId
      , const std::shared_ptr<spdlog::logger>& logger
      , const Tx &tx, QWidget* parent)
{
   auto dlg = std::make_shared<CreateTransactionDialogAdvanced>(topBlock, false
      , logger, nullptr, bs::UtxoReservationToken(), parent);

   dlg->setWindowTitle(tr("Child-Pays-For-Parent"));
   dlg->ui_->pushButtonImport->setEnabled(false);
   dlg->ui_->pushButtonShowSimple->setEnabled(false);

   //FIXME: dlg->setCPFPinputs(tx, wallet);
   dlg->isCPFP_ = true;
   return dlg;
}

std::shared_ptr<CreateTransactionDialog> CreateTransactionDialogAdvanced::CreateForPaymentRequest(
        uint32_t topBlock, const std::shared_ptr<spdlog::logger>& logger
      , const Bip21::PaymentRequestInfo& paymentInfo, QWidget* parent)
{
   auto dlg = std::make_shared<CreateTransactionDialogAdvanced>(topBlock, false
      , logger, nullptr, bs::UtxoReservationToken(), parent);

   dlg->ui_->pushButtonImport->setEnabled(false);
   dlg->ui_->pushButtonShowSimple->setEnabled(CreateTransactionDialog::canUseSimpleMode(paymentInfo));

   dlg->paymentInfo_ = paymentInfo;

   // set output
   const auto addr = bs::Address::fromAddressString(paymentInfo.address.toStdString());
   dlg->AddRecipient(CreateTransactionDialogAdvanced::Recipient{ addr, paymentInfo.amount});

   dlg->ui_->treeViewOutputs->setEnabled(false);

   dlg->ui_->lineEditAddress->setEnabled(false);
   dlg->ui_->pushButtonMax->setEnabled(false);
   dlg->ui_->lineEditAmount->setEnabled(false);

   // set fee
   if (!qFuzzyIsNull(paymentInfo.feePerByte)) {
      dlg->SetPredefinedFeeRate(paymentInfo.feePerByte);
   }

   // disable RBF for request
   if (!paymentInfo.requestURL.isEmpty()) {
      dlg->ui_->checkBoxRBF->setChecked(false);
      dlg->ui_->checkBoxRBF->setEnabled(false);
      dlg->ui_->checkBoxRBF->setToolTip(tr("RBF disabled for payment request"));

      dlg->nam_ = std::make_shared<QNetworkAccessManager>();
   }

   // set message or label to comment
   if (!paymentInfo.requestMemo.isEmpty()) {
      dlg->ui_->textEditComment->setText(paymentInfo.requestMemo);
   } else if (!paymentInfo.message.isEmpty()) {
      dlg->ui_->textEditComment->setText(paymentInfo.message);
   } else if (!paymentInfo.label.isEmpty()) {
      dlg->ui_->textEditComment->setText(paymentInfo.label);
   }

   dlg->validateAddOutputButton();
   return dlg;
}

void CreateTransactionDialogAdvanced::setCPFPinputs(const Tx &tx, const std::shared_ptr<bs::sync::Wallet> &wallet)
{
   std::set<BinaryData> txHashSet;
   std::map<BinaryData, std::set<uint32_t>> txOutIndices;
   for (size_t i = 0; i < tx.getNumTxIn(); i++) {
      const auto txin = tx.getTxInCopy(i);
      const auto outpoint = txin.getOutPoint();
      txHashSet.insert(outpoint.getTxHash());
      txOutIndices[outpoint.getTxHash()].insert(outpoint.getTxOutIndex());
   }
   allowAutoSelInputs_ = false;

   const auto &cbTXs = [this, tx, txOutIndices, cpfpWallet=wallet]
      (const AsyncClient::TxBatchResult &result, std::exception_ptr)
   {  //TODO: handle eptr!=null somehow
      auto selInputs = transactionData_->getSelectedInputs();
      selInputs->SetUseAutoSel(false);
      int64_t origFee = 0;
      unsigned int cntOutputs = 0;
      for (const auto &item : result) {
         if (!item.second || !item.second->isInitialized()) {
            SPDLOG_LOGGER_ERROR(logger_, "uninitialized TX received - skipping, tx hash: {}", item.first.toHexStr(true));
            continue;
         }
         const auto &prevTx = *item.second;
         const auto &txHash = prevTx.getThisHash();
         const auto &itTxOut = txOutIndices.find(txHash);
         if (itTxOut == txOutIndices.end()) {
            continue;
         }
         for (const auto &txOutIdx : itTxOut->second) {
            TxOut prevOut = prevTx.getTxOutCopy(txOutIdx);
            origFee += prevOut.getValue();
         }
      }

      for (size_t i = 0; i < tx.getNumTxOut(); i++) {
         auto out = tx.getTxOutCopy(i);
         const auto addr = bs::Address::fromTxOut(out);
#if 0
         const auto wallet = walletsManager_->getWalletByAddress(addr);
         if (wallet != nullptr && wallet->walletId() == cpfpWallet->walletId()) {
            if (selInputs->SetUTXOSelection(tx.getThisHash(),
               out.getIndex())) {
               cntOutputs++;
            }
         }
#endif
         origFee -= out.getValue();
      }

      if (!cntOutputs) {
         if (logger_ != nullptr) {
            logger_->error("[setCPFPinputs] No input(s) found for TX {}"
               , tx.getThisHash().toHexStr(true));
         }
         return;
      }
      if (origFee < 0) {
         if (logger_ != nullptr) {
            logger_->error("[setCPFPinputs] Negative TX balance ({}) for TX {}"
               , origFee, tx.getThisHash().toHexStr(true));
         }
         return;
      }

      const auto &cbFee = [this, tx, origFee, selInputs](float fee) {
         const auto txSize = tx.getTxWeight();
         const float feePerByte = (float)origFee / txSize;
         originalFee_ = origFee;
         originalFeePerByte_ = feePerByte;

         addedFee_ = (fee - feePerByte) * txSize;
         if (addedFee_ < 0) {
            addedFee_ = 0;
         }

         advisedFeePerByte_ = feePerByte + addedFee_ / txSize;
         populateFeeList();
         SetInputs(selInputs->GetSelectedTransactions());

         SetMinimumFee(originalFee_ + addedFee_, advisedFeePerByte_);
         onTransactionUpdated();
      };
      //walletsManager_->estimatedFeePerByte(2, cbFee, this);
   };

   SetFixedWallet(wallet->walletId(), [this, txHashSet, cbTXs] {
      //armory_->getTXsByHash(txHashSet, cbTXs, true);
   });
}

void CreateTransactionDialogAdvanced::setRBFinputs(const Tx &tx)
{
   isRBF_ = true;

   std::set<BinaryData> txHashSet;
   std::map<BinaryData, std::set<uint32_t>> txOutIndices;
   for (size_t i = 0; i < tx.getNumTxIn(); i++) {
      const TxIn txin = tx.getTxInCopy(i);
      const OutPoint outpoint = txin.getOutPoint();
      txHashSet.insert(outpoint.getTxHash());
      txOutIndices[outpoint.getTxHash()].insert(outpoint.getTxOutIndex());
   }

   const auto &cbTXs = [this, tx, txOutIndices]
      (const AsyncClient::TxBatchResult &result, std::exception_ptr)
   {  // TODO: handle eptr!=null somehow
      int64_t totalVal = 0;
      std::shared_ptr<bs::sync::hd::Group> inputsGroup;
      std::set<std::shared_ptr<bs::sync::Wallet>> inputWallets;
      std::vector<std::pair<BinaryData, unsigned int>> utxoSelected;
      for (const auto &item : result) {
         if (!item.second || !item.second->isInitialized()) {
            SPDLOG_LOGGER_ERROR(logger_, "uninitialized TX received - skipping, tx hash: {}", item.first.toHexStr(true));
            continue;
         }
         const auto &prevTx = *item.second;
         const auto &txHash = prevTx.getThisHash();
         const auto &itTxOut = txOutIndices.find(txHash);
         if (itTxOut == txOutIndices.end()) {
            continue;
         }
         for (const auto &txOutIdx : itTxOut->second) {
            const TxOut prevOut = prevTx.getTxOutCopy(txOutIdx);
            totalVal += prevOut.getValue();

            const auto addr = bs::Address::fromTxOut(prevOut);
#if 0
            const auto wallet = walletsManager_->getWalletByAddress(addr);
            if (wallet) {
               const auto group = walletsManager_->getGroupByWalletId(wallet->walletId());
               if (!inputsGroup) {
                  inputsGroup = group;
               }
               else {
                  if (inputsGroup != group) {
                     if (logger_) {
                        logger_->debug("[setRBFinputs] inputs group mismatch for {}", addr.display());
                     }
                     continue;
                  }
               }
               inputWallets.insert(wallet);
               utxoSelected.push_back({ txHash, txOutIdx });
            }
#endif
         }
      }

      auto walletsToWait = std::make_shared<std::set<std::string>>();
      auto rbfUtxos = std::make_shared<std::vector<UTXO>>();
      for (const auto &wallet : inputWallets) {
         walletsToWait->insert(wallet->walletId());
      }
      for (const auto &wallet : inputWallets) {
         const auto cbRBFUtxos = [this, walletId = wallet->walletId(), walletsToWait, rbfUtxos
            , inputsGroup, utxoSelected, tx, totalVal]
            (const std::vector<UTXO> &utxos) mutable
         {
            for (const auto &utxo : utxos) {
               const auto itFind = std::find_if(utxoSelected.cbegin(), utxoSelected.cend()
                  , [utxo](const std::pair<BinaryData, unsigned int> &sel)
               {
                  if ((sel.first == utxo.getTxHash()) && (sel.second == utxo.getTxOutIndex())) {
                     return true;
                  }
                  return false;
               });
               if (itFind == utxoSelected.end()) {
                  continue;
               }
               rbfUtxos->emplace_back(std::move(utxo));
            }

            walletsToWait->erase(walletId);
            if (walletsToWait->empty()) {
               const auto lbdSetInputs = [this, inputsGroup, utxoSelected, tx, totalVal, rbfUtxos]
                  () mutable
               {
                  setFixedGroupInputs(inputsGroup, *rbfUtxos);

                  for (const auto &sel : utxoSelected) {
                     if (!transactionData_->getSelectedInputs()->SetUTXOSelection(sel.first, sel.second)) {
                        if (logger_ != nullptr) {
                           logger_->warn("[CreateTransactionDialogAdvanced::setRBFinputs] no input(s) found for TX {}/{}"
                              , sel.first.toHexStr(true), sel.second);
                        }
                     }
                  }

                  const auto inputs = transactionData_->getSelectedInputs()->GetSelectedTransactions();
                  usedInputsModel_->updateInputs(inputs);

                  QString  changeAddress;

                  struct OwnOutput {
                     bs::Address    address;
                     bs::XBTAmount  amount;
                     bs::hd::Path   path;
                  };
                  std::vector<OwnOutput> ownOutputs;

                  // set outputs
                  for (size_t i = 0; i < tx.getNumTxOut(); i++) {
                     TxOut out = tx.getTxOutCopy(i);
                     const auto addr = bs::Address::fromTxOut(out);
                     bs::XBTAmount amount{(int64_t)out.getValue()};
#if 0
                     const auto wallet = walletsManager_->getWalletByAddress(addr);
                     if (wallet) {
                        ownOutputs.push_back({ addr, amount
                           , bs::hd::Path::fromString(wallet->getAddressIndex(addr)) });
                     } else
#endif
                     {
                        AddRecipient({ addr, amount});
                     }
                     totalVal -= out.getValue();
                  }

                  // Assume change address is the last internal address in the
                  // list of outputs belonging to the wallet
                  for (const auto &output : ownOutputs) {
                     const auto path = output.path;

                     bool isChange = false;

                     if (changeAddress.isEmpty() ) {
                        if (path.length() == 2) {
                           if (path.get(-2) == 1) {   // internal HD address
                              isChange = true;
                           }
                        } else {   // not an HD wallet/address
                           isChange = true;
                        }
                     }

                     if (isChange) {
                        changeAddress = QString::fromStdString(output.address.display());
                     } else {
                        AddRecipient({ output.address, output.amount });
                     }
                  }

                  // Error check.
                  if (totalVal < 0) {
                     if (logger_ != nullptr) {
                        logger_->error("[CreateTransactionDialogAdvanced::setRBFinputs] Negative TX balance ({}) for TX {}."
                           , totalVal
                           , tx.getThisHash().toHexStr(true));
                     }
                     return;
                  }

                  // If we did find a change address, set it in place in this TX.
                  else if (!changeAddress.isEmpty()) {
                     // If the original TX didn't use up the original inputs, force the
                     // original change address to be used. It may be desirable to change
                     // this eventually.
                     SetFixedChangeAddress(changeAddress);
                  }

                  // RBF minimum amounts are a little tricky. The rules/policies are:
                  //
                  // - RULE: Calculate based not on the absolute TX size, but on the virtual
                  //   size, which is ceil(TX weight / 4) (e.g., 32.2 -> 33). For reference,
                  //   TX weight = Total_TX_Size + (3 * Base_TX_Size).
                  //   (Base_TX_Size = TX size w/o witness data)
                  // - RULE: The new fee/KB must meet or exceed the old one. (If replacing
                  //   multiple TXs, Core seems to calculate based on the sum of fees and
                  //   TX sizes for the old TXs.)
                  // - RULE: The new fee must be at least 1 satoshi higher than the sum of
                  //   the fees of the replaced TXs.
                  // - POLICY: The new fee must be bumped by, at a minimum, the incremental
                  //   relay fee (IRL) * the new TX's virtual size. The fee can be adjusted
                  //   in Core by the incrementalrelayfee config option. By default, the fee
                  //   is 1000 sat/KB (1 sat/B), which is what we will assume is being used.
                  //   (This may need to be a terminal config option later.) So, if the virt
                  //   size is 146, and the original fee is 1 sat/b (146 satoshis), the next
                  //   fee must be at least 2 sat/b (292 satoshis), then 3 sat/b, etc. This
                  //   assumes we don't change the TX in any way. Bumping to a virt size of
                  //   300 would require the 1st RBF to be 446 satoshis, then 746, 1046, etc.
                  //
                  // It's impossible to calculate the minimum required fee, as the user can
                  // do many different things. We'll just start by setting the minimum fee
                  // to the amount required by the RBF/IRL policy, and keep the minimum
                  // fee/byte where it is.
                  originalFee_ = totalVal;
                  const float feePerByte = (float)totalVal / (float)tx.getTxWeight();
                  originalFeePerByte_ = feePerByte;
                  const uint64_t newMinFee = originalFee_ + tx.getTxWeight();
                  SetMinimumFee(newMinFee, originalFeePerByte_ + 1.0);
                  advisedFeePerByte_ = originalFeePerByte_ + 1.0;

                  onFeeSuggestionsLoaded({});

                  SetInputs(transactionData_->getSelectedInputs()->GetSelectedTransactions());
               };
               QMetaObject::invokeMethod(this, lbdSetInputs);
            }
         };
         wallet->getRBFTxOutList(cbRBFUtxos);
      }
   };
   //armory_->getTXsByHash(txHashSet, cbTXs, true);
}

void CreateTransactionDialogAdvanced::onUpdateChangeWidget()
{
   if (changeAddressFixed_) {
      return;
   }

   auto walletType = UiUtils::getSelectedWalletType(comboBoxWallets());

   ui_->radioButtonExistingAddress->setVisible(false);
   ui_->radioButtonNewAddrNative->setVisible(false);
   ui_->radioButtonNewAddrNested->setVisible(false);
   ui_->radioButtonNewAddrLegacy->setVisible(false);

   switch (walletType)
   {
   case UiUtils::WalletsTypes::Full:
   case UiUtils::WalletsTypes::WatchOnly:
   {
      ui_->radioButtonExistingAddress->setVisible(true);
      ui_->radioButtonNewAddrNative->setVisible(true);
      ui_->radioButtonNewAddrNested->setVisible(true);
      ui_->radioButtonNewAddrNative->setChecked(true);
   }
   break;
   case UiUtils::WalletsTypes::HardwareLegacy:
      ui_->radioButtonNewAddrLegacy->setVisible(true);
      ui_->radioButtonNewAddrLegacy->setChecked(true);
      break;
   case UiUtils::WalletsTypes::HardwareNativeSW:
   {
      ui_->radioButtonNewAddrNative->setVisible(true);
      ui_->radioButtonNewAddrNative->setChecked(true);
      break;
   }
   case UiUtils::WalletsTypes::HardwareNestedSW:
   {
      ui_->radioButtonNewAddrNested->setVisible(true);
      ui_->radioButtonNewAddrNested->setChecked(true);
      break;
   }
   default:
      break;
   }

   ui_->widgetChangeAddress->setEnabled(!(walletType & UiUtils::WalletsTypes::HardwareAll));
}

void CreateTransactionDialogAdvanced::initUI()
{
   usedInputsModel_ = new UsedInputsModel(this);
   outputsModel_ = new TransactionOutputsModel(this);

   CreateTransactionDialog::init();

   ui_->treeViewInputs->setModel(usedInputsModel_);
   ui_->treeViewInputs->setColumnWidth(1, 50);
   ui_->treeViewInputs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
   ui_->treeViewInputs->header()->setSectionResizeMode(1, QHeaderView::Fixed);
   ui_->treeViewInputs->header()->setSectionResizeMode(0, QHeaderView::Stretch);

   ui_->treeViewOutputs->setModel(outputsModel_);
   ui_->treeViewOutputs->setColumnWidth(2, 30);
   ui_->treeViewOutputs->header()->setSectionResizeMode(2, QHeaderView::Fixed);
   ui_->treeViewOutputs->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
   ui_->treeViewOutputs->header()->setSectionResizeMode(0, QHeaderView::Stretch);

   // QModelIndex isn't used. We should use it or lose it.
   connect(ui_->treeViewOutputs, &QTreeView::clicked, this, &CreateTransactionDialogAdvanced::onOutputsClicked);
   connect(outputsModel_, &TransactionOutputsModel::rowsRemoved, [this](const QModelIndex &parent, int first, int last) {
      onOutputRemoved(first);
   });

   currentAddress_.clear();
   currentValue_ = 0;
   if (qFuzzyIsNull(minFeePerByte_) || qFuzzyIsNull(minTotalFee_)) {
      SetMinimumFee(0, 1.0);
   }

   ui_->pushButtonAddOutput->setEnabled(false);
   ui_->line->hide();
   ui_->pushButtonSelectInputs->setEnabled(ui_->comboBoxWallets->count() > 0);

   connect(ui_->lineEditAddress, &QLineEdit::textChanged, this, &CreateTransactionDialogAdvanced::onAddressTextChanged);
   connect(ui_->lineEditAmount, &QLineEdit::textChanged, this, &CreateTransactionDialogAdvanced::onXBTAmountChanged);
   ui_->lineEditAddress->installEventFilter(this);
   ui_->lineEditAmount->installEventFilter(this);

   connect(ui_->pushButtonSelectInputs, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onSelectInputs);
   connect(ui_->pushButtonAddOutput, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onAddOutput);
   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onCreatePressed);
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onImportPressed);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::reject);
   connect(ui_->pushButtonShowSimple, &QPushButton::clicked, this, &CreateTransactionDialogAdvanced::onSimpleDialogRequested);

   ui_->radioButtonNewAddrNative->setChecked(true);

   connect(ui_->radioButtonNewAddrNative, &QRadioButton::clicked, this, &CreateTransactionDialogAdvanced::onNewAddressSelectedForChange);
   connect(ui_->radioButtonNewAddrNested, &QRadioButton::clicked, this, &CreateTransactionDialogAdvanced::onNewAddressSelectedForChange);
   connect(ui_->radioButtonExistingAddress, &QRadioButton::clicked, this, &CreateTransactionDialogAdvanced::onExistingAddressSelectedForChange);

   ui_->treeViewOutputs->setContextMenuPolicy(Qt::CustomContextMenu);

   removeOutputAction_ = new QAction(tr("Remove Output"), this);
   connect(removeOutputAction_, &QAction::triggered, this, &CreateTransactionDialogAdvanced::onRemoveOutput);
   contextMenu_.addAction(removeOutputAction_);

   connect(ui_->treeViewOutputs, &QTreeView::customContextMenuRequested, this, &CreateTransactionDialogAdvanced::showContextMenu);

   connect(ui_->doubleSpinBoxFeesManualPerByte, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
      , this, &CreateTransactionDialogAdvanced::setTxFees);
   connect(ui_->spinBoxFeesManualTotal, QOverload<int>::of(&QSpinBox::valueChanged)
      , this, &CreateTransactionDialogAdvanced::setTxFees);

   // Signal from parent class
   connect(this, &CreateTransactionDialogAdvanced::walletChanged, this, &CreateTransactionDialogAdvanced::onUpdateChangeWidget);

   updateManualFeeControls();
   onUpdateChangeWidget();
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
         if ((watched == ui_->lineEditAddress) && currentAddress_.isValid() && qFuzzyIsNull(currentValue_)) {
            ui_->lineEditAmount->setFocus();
         }
         if ((watched == ui_->lineEditAmount) && !qFuzzyIsNull(currentValue_) && !currentAddress_.isValid()) {
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

void CreateTransactionDialogAdvanced::onWalletsList(const std::string& id
   , const std::vector<bs::sync::HDWalletData>& wallets)
{
   CreateTransactionDialog::onWalletsList(id, wallets);
   ui_->pushButtonSelectInputs->setEnabled(ui_->comboBoxWallets->count() > 0);
}

void CreateTransactionDialogAdvanced::onOutputsClicked(const QModelIndex &index)
{
   if (!index.isValid()) {
      return;
   }

   if (!removeOutputEnabled_) {
      return;
   }

   if (outputsModel_->isRemoveColumn(index.column())) {
      const auto &outputId = outputsModel_->GetOutputId(index.row());
      RemoveOutputByRow(outputsModel_->GetRowById(outputId));
   }
   else {
      outputRow_ = index.row();
      const auto &outputId = outputsModel_->GetOutputId(outputRow_);

      currentAddress_ = transactionData_->GetRecipientAddress(outputId);
      ui_->lineEditAddress->setText(QString::fromStdString(currentAddress_.display()));
      ui_->lineEditAmount->setText(UiUtils::displayAmount(transactionData_->GetRecipientAmount(outputId)));
      ui_->lineEditAmount->setFocus();
      updateOutputButtonTitle();
   }
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

void CreateTransactionDialogAdvanced::onOutputRemoved(int rowNumber)
{
   if (outputRow_ >= 0) {
      if (rowNumber < outputRow_) {
         --outputRow_;
      } else if (rowNumber == outputRow_) {
         outputRow_ = -1;
      }

      if (outputRow_ == -1) {
         ui_->lineEditAddress->clear();
         ui_->lineEditAmount->clear();

         ui_->treeViewOutputs->clearSelection();
      }
   }

   for (const auto &recipId : transactionData_->allRecipientIds()) {
      UpdateRecipientAmount(recipId, transactionData_->GetRecipientAmount(recipId), false);
   }
   transactionData_->setTotalFee(0, false);
   setTxFees();
   enableFeeChanging();
}

void CreateTransactionDialogAdvanced::RemoveOutputByRow(int row)
{
   auto outputId = outputsModel_->GetOutputId(row);

   transactionData_->RemoveRecipient(outputId);
   outputsModel_->RemoveRecipient(row);

   ui_->comboBoxFeeSuggestions->setEnabled(true);
}

void CreateTransactionDialogAdvanced::onTransactionUpdated()
{
   fixFeePerByte();
   CreateTransactionDialog::onTransactionUpdated();

   // If RBF is active, prevent the inputs from being changed. It may be
   // desirable to change this one day. RBF TXs can change inputs but only if
   // all other inputs are RBF-enabled. Properly refactored, the user could
   // select only RBF-enabled inputs that are waiting for a conf.
   if (!isRBF_ && transactionData_->getSelectedInputs()
      && transactionData_->getSelectedInputs()->UseAutoSel()) {
      usedInputsModel_->updateInputs(transactionData_->inputs());
   }

   const auto &summary = transactionData_->GetTransactionSummary();

   if (!changeAddressFixed_) {
      bool changeSelectionEnabled = summary.hasChange || (summary.txVirtSize == 0);
      ui_->changeAddrGroupBox->setEnabled(changeSelectionEnabled);
      showExistingChangeAddress(changeSelectionEnabled);
   }

   if (transactionData_->totalFee() && summary.txVirtSize
      && (transactionData_->totalFee() < summary.txVirtSize)) {
      transactionData_->setTotalFee(summary.txVirtSize);
      ui_->spinBoxFeesManualTotal->setValue(summary.txVirtSize);
   }

   if ((addedFee_ > 0) && !isCPFP_) {
      const float newTotalFee = transactionData_->feePerByte() * summary.txVirtSize + addedFee_;
      const float newFeePerByte = newTotalFee / summary.txVirtSize;
      if (!qFuzzyCompare(newTotalFee, advisedTotalFee_) || !qFuzzyCompare(newFeePerByte, advisedFeePerByte_)) {
         QMetaObject::invokeMethod(this, [this, newTotalFee, newFeePerByte] {
            setAdvisedFees(newTotalFee, newFeePerByte);
            validateCreateButton();
         });
         return;
      }
   }

   QMetaObject::invokeMethod(this, &CreateTransactionDialogAdvanced::validateAmountLine
      , Qt::QueuedConnection);

   QMetaObject::invokeMethod(this, &CreateTransactionDialogAdvanced::validateCreateButton
      , Qt::QueuedConnection);
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
      const auto &addr = bs::Address::fromAddressString(addressString.trimmed().toStdString());
      if (addr != currentAddress_) {
         outputRow_ = -1;
         if (addr.format() == bs::Address::Format::Hex) {
            currentAddress_.clear();   // P2WSH unprefixed address can resemble TX hash,
         }                             // so we disable hex format completely
         else {
            currentAddress_ = addr;
         }
      }
   } catch (...) {
      outputRow_ = -1;
      currentAddress_.clear();
   }
   UiUtils::setWrongState(ui_->lineEditAddress, !currentAddress_.isValid());

   validateAddOutputButton();
}

void CreateTransactionDialogAdvanced::onXBTAmountChanged(const QString &text)
{
   currentValue_ = UiUtils::parseAmountBtc(text);
   validateAddOutputButton();
}

void CreateTransactionDialogAdvanced::onSelectInputs()
{
   const uint64_t prevBalance = transactionData_->GetTransactionSummary().availableBalance * BTCNumericTypes::BalanceDivider;
   const auto &spendBalance = transactionData_->GetTotalRecipientsAmount();
   const uint64_t totalFee = transactionData_->GetTransactionSummary().totalFee;
   CoinControlDialog dlg(transactionData_->getSelectedInputs(), allowAutoSelInputs_, this);
   if (dlg.exec() == QDialog::Accepted) {
      SetInputs(dlg.selectedInputs());
   }

   const uint64_t curBalance = transactionData_->GetTransactionSummary().availableBalance * BTCNumericTypes::BalanceDivider;
   if (curBalance < (spendBalance.GetValue() + totalFee)) {
      BSMessageBox lowInputs(BSMessageBox::question, tr("Insufficient Input Amount")
         , tr("The output[s] exceed the value of the input[s]. Do you wish to delete some output[s]?"));
      if (lowInputs.exec() == QDialog::Accepted) {
         while (outputsModel_->rowCount({})) {
            RemoveOutputByRow(0);
            if (curBalance >= (transactionData_->GetTotalRecipientsAmount().GetValue()
               + transactionData_->GetTransactionSummary().totalFee)) {
               break;
            }
         }
      }
   }
   else if (curBalance > prevBalance) {
      enableFeeChanging();
   }
}

void CreateTransactionDialogAdvanced::onAddOutput()
{
   const bs::XBTAmount curVal{ currentValue_ };
   if (outputRow_ >= 0) {
      const auto &outputId = outputsModel_->GetOutputId(outputRow_);
      UpdateRecipientAmount(outputId, curVal, false);

      outputRow_ = -1;
      ui_->lineEditAddress->clear();
      ui_->lineEditAmount->clear();
      ui_->lineEditAddress->setFocus();

      ui_->treeViewOutputs->clearSelection();
   }
   else {
      currentAddress_ = bs::Address::fromAddressString(ui_->lineEditAddress->text().trimmed().toStdString());
      const auto &maxValue = transactionData_->CalculateMaxAmount(currentAddress_);
      bool maxAmount = std::abs(maxValue - curVal
         - transactionData_->GetTotalRecipientsAmount()) <= 1;

      AddRecipient({ currentAddress_, curVal, maxAmount });

      maxAmount |= FixRecipientsAmount();

      if (maxAmount) {
         enableFeeChanging(false);
      }
      outputRow_ = -1;
      ui_->lineEditAddress->clear();
      ui_->lineEditAmount->clear();
      ui_->lineEditAddress->setFocus();
   }
   validateAddOutputButton();
}

void CreateTransactionDialogAdvanced::updateOutputButtonTitle()
{
   if (outputRow_ >= 0) {
      ui_->pushButtonAddOutput->setText(tr("Update output"));
   }
   else {
      ui_->pushButtonAddOutput->setText(tr("Include output"));
   }
}

// Nothing is being done with isMax right now. We should use it or lose it.
unsigned int CreateTransactionDialogAdvanced::AddRecipient(const Recipient &recip)
{
   const auto recipientId = transactionData_->RegisterNewRecipient();
   transactionData_->UpdateRecipientAddress(recipientId, recip.address);
   transactionData_->UpdateRecipientAmount(recipientId, recip.amount, recip.isMax);

   // add to the model
   outputsModel_->AddRecipient(recipientId
      , QString::fromStdString(recip.address.display()), recip.amount);

   return recipientId;
}

void CreateTransactionDialogAdvanced::AddRecipients(const std::vector<Recipient> &recipients)
{
   std::vector<std::tuple<unsigned int, QString, bs::XBTAmount>> modelRecips;
   for (const auto &recip : recipients) {
      const auto recipientId = transactionData_->RegisterNewRecipient();
      transactionData_->UpdateRecipientAddress(recipientId, recip.address);
      transactionData_->UpdateRecipientAmount(recipientId, recip.amount, recip.isMax);
      modelRecips.push_back({recipientId, QString::fromStdString(recip.address.display())
         , recip.amount});
   }
   QMetaObject::invokeMethod(outputsModel_, [this, modelRecips] { outputsModel_->AddRecipients(modelRecips); });
}

void CreateTransactionDialogAdvanced::onMaxPressed()
{
   CreateTransactionDialog::onMaxPressed();

   if (outputRow_ >= 0) {
      const auto maxValue = transactionData_->CalculateMaxAmount({});
      if (maxValue.GetValue() > 0) {
         const auto &outputId = outputsModel_->GetOutputId(outputRow_);
         const auto prevValue = transactionData_->GetRecipientAmount(outputId);
         lineEditAmount()->setText(UiUtils::displayAmount(maxValue + prevValue));
      }
   }
}

// Attempts to remove the change if it's small enough and adds its amount to fees
bool CreateTransactionDialogAdvanced::FixRecipientsAmount()
{
   if (!transactionData_->totalFee()) {
      return false;
   }
   const uint64_t totalFee = transactionData_->totalFee();
   int64_t diffMax = transactionData_->GetTransactionSummary().availableBalance * BTCNumericTypes::BalanceDivider
      - transactionData_->GetTotalRecipientsAmount().GetValue() - totalFee;
   const uint64_t newTotalFee = diffMax + totalFee;

   size_t maxOutputSize = 0;
   for (const auto &recipId : transactionData_->allRecipientIds()) {
      const auto recip = transactionData_->GetScriptRecipient(recipId);
      if (!recip) {
         continue;
      }
      maxOutputSize = std::max(maxOutputSize, recip->getSize());
   }
   if (!maxOutputSize) {
      maxOutputSize = totalFee / kDustFeePerByte / 2; // fallback if failed to get any recipients size
   }

   if (diffMax < 0) {
      diffMax = 0;
   }
   // The code below tries to eliminate the change address if the change amount is too little (less than half of current fee).
   if ((diffMax >= 0.00000001) && (diffMax <= (maxOutputSize * kDustFeePerByte / BTCNumericTypes::BalanceDivider))) {
      BSMessageBox question(BSMessageBox::question, tr("Change fee")
         , tr("Your projected change amount %1 is too small as compared to the projected fee."
            " Attempting to keep the change will prevent the transaction from being propagated through"
            " the Bitcoin network.").arg(UiUtils::displayAmount(diffMax))
         , tr("Would you like to remove the change output and put its amount towards the fees?")
         , this);
      if (question.exec() == QDialog::Accepted) {
         transactionData_->setTotalFee(newTotalFee * BTCNumericTypes::BalanceDivider, false);
         for (const auto &recipId : transactionData_->allRecipientIds()) {
            UpdateRecipientAmount(recipId, transactionData_->GetRecipientAmount(recipId), true);
         }
         return true;
      }
   }
   else if (diffMax < 0.00000001) {   // if diff is less than 1 satoshi (which can be caused by maxAmount calc tolerance)
      for (const auto &recipId : transactionData_->allRecipientIds()) {
         UpdateRecipientAmount(recipId, transactionData_->GetRecipientAmount(recipId), true);
      }
      return true;
   }
   return false;
}

void CreateTransactionDialogAdvanced::UpdateRecipientAmount(unsigned int recipId
   , const bs::XBTAmount &amount, bool isMax)
{
   transactionData_->UpdateRecipientAmount(recipId, amount, isMax);
   outputsModel_->UpdateRecipientAmount(recipId, amount);
}

void CreateTransactionDialogAdvanced::setValidationStateOnAmount(bool isValid)
{
   UiUtils::setWrongState(ui_->lineEditAmount, !isValid);
}

bool CreateTransactionDialogAdvanced::isCurrentAmountValid() const
{
   const bs::XBTAmount curVal{ currentValue_ };
   if (!curVal.GetValue()) {
      return false;
   }
   bs::XBTAmount maxAmount;
   if (outputRow_ >= 0) {
      const auto &outputId = outputsModel_->GetOutputId(outputRow_);
      const auto &prevValue = transactionData_->GetRecipientAmount(outputId);
      maxAmount = transactionData_->CalculateMaxAmount({}) + prevValue;
   }
   else {
      maxAmount = transactionData_->CalculateMaxAmount(currentAddress_);
   }
   if ((maxAmount - curVal) < -1) {  // 1 satoshi difference is allowed due to rounding error
      return false;
   }
   return true;
}

void CreateTransactionDialogAdvanced::validateAddOutputButton()
{
   updateOutputButtonTitle();
   ui_->pushButtonMax->setEnabled(currentAddress_.isValid());

   const bs::XBTAmount curVal{ currentValue_ };
   bool hasAmountChanged = true;
   if (outputRow_ >= 0) {
      const auto &outputId = outputsModel_->GetOutputId(outputRow_);
      const auto &prevValue = transactionData_->GetRecipientAmount(outputId);
      if (prevValue == curVal) {
         hasAmountChanged = false;
      }
   }

   const bool amountIsValid = isCurrentAmountValid();

   setValidationStateOnAmount(amountIsValid);

   ui_->pushButtonAddOutput->setEnabled(currentAddress_.isValid()
      && hasAmountChanged && amountIsValid);
}

void CreateTransactionDialogAdvanced::validateCreateButton()
{
   const bool isTxValid = transactionData_->IsTransactionValid() && transactionData_->GetTransactionSummary().txVirtSize;

   updateCreateButtonText();
   ui_->pushButtonCreate->setEnabled(isTxValid
      && !broadcasting_
      && (ui_->radioButtonNewAddrNative->isChecked() || ui_->radioButtonNewAddrNested->isChecked()
         || ui_->radioButtonNewAddrLegacy->isChecked() || (selectedChangeAddress_.isValid())));
}

void CreateTransactionDialogAdvanced::SetInputs(const std::vector<UTXO> &inputs)
{
   usedInputsModel_->updateInputs(inputs);

   const auto maxAmt = transactionData_->CalculateMaxAmount();
   const auto &recipSumAmt = transactionData_->GetTotalRecipientsAmount();
   if (maxAmt != recipSumAmt) {
      for (const auto &recip : transactionData_->allRecipientIds()) {
         const auto recipAmt = transactionData_->GetRecipientAmount(recip);
         transactionData_->UpdateRecipientAmount(recip, recipAmt, false);
      }
   }
}

void CreateTransactionDialogAdvanced::AddManualFeeEntries(float feePerByte, float totalFee)
{
   ui_->doubleSpinBoxFeesManualPerByte->setValue(feePerByte);
   ui_->spinBoxFeesManualTotal->setValue(qRound(totalFee));
   ui_->comboBoxFeeSuggestions->addItem(tr("Manual Fee Selection"));
   ui_->comboBoxFeeSuggestions->addItem(tr("Total Network Fee"));
}

void CreateTransactionDialogAdvanced::setAdvisedFees(float totalFee, float feePerByte)
{
   advisedTotalFee_ = totalFee;
   advisedFeePerByte_ = feePerByte;

   if (advisedFeePerByte_ > 0) {
      const auto index = ui_->comboBoxFeeSuggestions->count() - 2;
      if (qFuzzyIsNull(advisedTotalFee_)) {
         ui_->comboBoxFeeSuggestions->setCurrentIndex(index);
         feeSelectionChanged(index);
      }
   }

   if (advisedTotalFee_ > 0) {
      const auto index = ui_->comboBoxFeeSuggestions->count() - 1;
      ui_->comboBoxFeeSuggestions->setCurrentIndex(index);
      feeSelectionChanged(index);
   }
}

void CreateTransactionDialogAdvanced::onFeeSuggestionsLoaded(const std::map<unsigned int, float> &feeValues)
{
   if (feeChangeDisabled_) {
      return;
   }

   CreateTransactionDialog::onFeeSuggestionsLoaded(feeValues);

   float manualFeePerByte = advisedFeePerByte_;
   if (manualFeePerByte < minFeePerByte_) {
      manualFeePerByte = minFeePerByte_;
   }
   if (qFuzzyIsNull(manualFeePerByte)) {
      manualFeePerByte = feeValues.begin()->second;
   }
   AddManualFeeEntries(manualFeePerByte
      , (minTotalFee_ > 0) ? minTotalFee_ : transactionData_->totalFee());

   setAdvisedFees(advisedTotalFee_, advisedFeePerByte_);

   if (feeValues.empty()) {
      feeSelectionChanged(0);
   }

   if (!importedTxTotalFee_.isZero()) {
      SetPredefinedFee(importedTxTotalFee_.GetValue());
   }
}

void CreateTransactionDialogAdvanced::SetMinimumFee(float totalFee, float feePerByte)
{
   minTotalFee_ = totalFee;
   minFeePerByte_ = feePerByte;

   ui_->doubleSpinBoxFeesManualPerByte->setMinimum(feePerByte);
   ui_->spinBoxFeesManualTotal->setMinimum(qRound(totalFee));

   transactionData_->setMinTotalFee(minTotalFee_);
}

// currentIndex isn't being used. We should use it or lose it.
void CreateTransactionDialogAdvanced::feeSelectionChanged(int currentIndex)
{
   updateManualFeeControls();
   setTxFees();
}

void CreateTransactionDialogAdvanced::getChangeAddress(AddressCb cb)
{
   if (transactionData_->GetTransactionSummary().hasChange) {
      if (changeAddressFixed_) {
         cb(selectedChangeAddress_);
         return;
      }
      else if (ui_->radioButtonNewAddrNative->isChecked() || ui_->radioButtonNewAddrNested->isChecked()
         || ui_->radioButtonNewAddrLegacy->isChecked()) {
#if 0
         const auto group = transactionData_->getGroup();
         std::shared_ptr<bs::sync::Wallet> wallet;
         if (group) {
            const auto purpose = ui_->radioButtonNewAddrNative->isChecked()
               ? bs::hd::Purpose::Native : bs::hd::Purpose::Nested;
            wallet = group->getLeaf(bs::hd::Path({ purpose
               , bs::sync::hd::Wallet::getXBTGroupType(), 0 }));
         }
         if (!wallet) {
            wallet = transactionData_->getWallet();
         }
         const auto &cbAddr = [this, cb = std::move(cb), wallet, handle = validityFlag_.handle()](const bs::Address &addr) {
            if (!handle.isValid()) {
               return;
            }
            logger_->debug("[CreateTransactionDialogAdvanced::getChangeAddress] new change address: {}"
               , addr.display());
            wallet->setAddressComment(addr
               , bs::sync::wallet::Comment::toString(bs::sync::wallet::Comment::ChangeAddress));
            //FIXME: transactionData_->getWallet()->syncAddresses();
            cb(addr);
         };
         wallet->getNewChangeAddress(cbAddr);
#endif   //0
         CreateTransactionDialog::getChangeAddress(cb);
         return;
      } else {
         cb(selectedChangeAddress_);
         return;
      }
   }
   cb({});
}

void CreateTransactionDialogAdvanced::onCreatePressed()
{
   if (!importedSignedTX_.empty()) {
      if (showUnknownWalletWarning_) {
         int rc = BSMessageBox(BSMessageBox::question, tr("Transaction Broadcast")
            , tr("Transaction Broadcast")
            , tr("The Terminal does not have access to the underlying wallet. Once the transaction is broadcast, you will only be able to view it in the explorer.\n\n"
            "Do you want to proceed with broadcasting?"
            )).exec();
         if (rc == QDialog::Rejected) {
            return;
         }
      }

      if (BroadcastImportedTx()) {
         accept();
      }
      else {
         initUI();
         validateCreateButton();
      }
      return;
   }

   CreateTransaction([this, handle = validityFlag_.handle()]
      (bool result, const std::string &errorMsg, const std::string& unsignedTx, uint64_t virtSize) {
      if (!handle.isValid()) {
         return;
      }
      if (!result) {
         BSMessageBox(BSMessageBox::critical, tr("Transaction")
            , tr("Transaction error"), QString::fromStdString(errorMsg)).exec();
         reject();
      }
      createTransactionImpl();
   });
}

bool CreateTransactionDialogAdvanced::HaveSignedImportedTransaction() const
{
   return !importedSignedTX_.empty();
}

void CreateTransactionDialogAdvanced::SetImportedTransactions(const std::vector<bs::core::wallet::TXSignRequest>& transactions)
{
   broadcasting_ = false;
   transactionData_->clear();
   transactionData_->getSelectedInputs()->SetUseAutoSel(false);
   selectedChangeAddress_.clear();
   ui_->labelChangeAddress->clear();
   QPointer<CreateTransactionDialogAdvanced> thisPtr = this;

   ui_->pushButtonCreate->setEnabled(false);
   ui_->pushButtonCreate->setText(tr("Broadcast"));

   ui_->pushButtonShowSimple->setEnabled(false);

   const auto &tx = transactions.at(0);
   if (!tx.serializedTx.empty()) {    // signed TX
      ui_->textEditComment->insertPlainText(QString::fromStdString(tx.comment));

      importedSignedTX_ = tx.serializedTx;
      Tx tx;
      try {
         tx = Tx(importedSignedTX_);
         if (!tx.isInitialized()) {
            throw std::runtime_error("invalid TX data");
         }
      }
      catch (const BlockDeserializingException &e) {
         // BlockDeserializingException sometimes does not have meaningful details
         SPDLOG_LOGGER_ERROR(logger_, "TX import failed: BlockDeserializingException: '{}'", e.what());
         BSMessageBox(BSMessageBox::critical, tr("Transaction import")
            , tr("Deserialization failed")).exec();
         return;
      }
      catch (const std::exception &e) {
         SPDLOG_LOGGER_ERROR(logger_, "TX import failed: '{}'", e.what());
         BSMessageBox(BSMessageBox::critical, tr("Transaction import")
            , tr("Import failed"), QString::fromStdString(e.what())).exec();
         return;
      }

      ui_->pushButtonCreate->setEnabled(true);

      std::set<BinaryData> txHashSet;
      std::map<BinaryData, std::set<uint32_t>> txOutIndices;
      TransactionData::UtxoHashes utxoHashes;

      for (size_t i = 0; i < tx.getNumTxIn(); i++) {
         auto in = tx.getTxInCopy((int)i);
         OutPoint op = in.getOutPoint();
         utxoHashes.push_back({ op.getTxHash(), op.getTxOutIndex() });
         txHashSet.insert(op.getTxHash());
         txOutIndices[op.getTxHash()].insert(op.getTxOutIndex());
      }

      const auto &cbTXs = [thisPtr, tx, utxoHashes, txOutIndices]
         (const AsyncClient::TxBatchResult &result, std::exception_ptr exPtr)
      {
         if (!thisPtr || exPtr) {
            return;
         }

         std::shared_ptr<bs::sync::Wallet> wallet;
         int64_t totalVal = 0;
         std::vector<UTXO> utxos;

         for (const auto &item : result) {
            if (!item.second || !item.second->isInitialized()) {
               SPDLOG_LOGGER_ERROR(thisPtr->logger_, "uninitialized TX received - skipping, tx hash: {}", item.first.toHexStr(true));
               continue;
            }
            const auto &prevTx = *item.second;
            const auto &itTxOut = txOutIndices.find(prevTx.getThisHash());
            if (itTxOut == txOutIndices.end()) {
               continue;
            }
            for (const auto &txOutIdx : itTxOut->second) {
               auto prevOut = prevTx.getTxOutCopy(txOutIdx);
               const auto &addr = bs::Address::fromTxOut(prevOut);
               UTXO utxo(prevOut.getValue(), prevTx.getTxHeight(), prevTx.getTxIndex(), txOutIdx
                  , prevTx.getThisHash(), prevOut.getScript());
               utxos.push_back(utxo);
               totalVal += prevOut.getValue();
               if (!wallet) {
#if 0
                  const auto &addrWallet = thisPtr->walletsManager_->getWalletByAddress(addr);
                  if (addrWallet) {
                     wallet = addrWallet;
                  }
#endif
               }
            }
         }

         thisPtr->transactionData_->setFixedInputs(utxos, tx.getTxWeight());
         if (wallet) {
            auto cbInputsReceived = [thisPtr, utxos] {
               if (!thisPtr) {
                  return;
               }
               thisPtr->usedInputsModel_->updateInputs(utxos);
            };
            thisPtr->ui_->comboBoxWallets->show();
            thisPtr->ui_->labelBalance->show();
            thisPtr->ui_->labelBal->show();
            thisPtr->ui_->labelWallet->show();
            thisPtr->showUnknownWalletWarning_ = false;
            thisPtr->SetFixedWallet(wallet->walletId(), cbInputsReceived);
         } else {
            thisPtr->showUnknownWalletWarning_ = true;
            thisPtr->ui_->comboBoxWallets->hide();
            // labelBalance will still update balance, let's hide it
            thisPtr->ui_->labelBalance->hide();
            thisPtr->ui_->labelBal->hide();
            thisPtr->ui_->labelWallet->hide();
         }
         thisPtr->usedInputsModel_->updateInputs(utxos);

         std::vector<Recipient> recipients;
         for (int i = tx.getNumTxOut() - 1; i >= 0; --i) {
            TxOut out = tx.getTxOutCopy(i);
            const auto addr = bs::Address::fromTxOut(out);
            if (thisPtr->selectedChangeAddress_.empty() && wallet && (wallet->containsAddress(addr))) {
               thisPtr->SetFixedChangeAddress(QString::fromStdString(addr.display()));
            }
            else {
               recipients.push_back({ addr, bs::XBTAmount{(int64_t)out.getValue()}, false });
            }
            totalVal -= out.getValue();
         }
         thisPtr->SetPredefinedFee(totalVal);
         if (!recipients.empty()) {
            std::reverse(recipients.begin(), recipients.end());
            thisPtr->AddRecipients(recipients);
         }
      };
      //armory_->getTXsByHash(txHashSet, cbTXs, true);
   }
   else { // unsigned TX
      importedSignedTX_.clear();
      if (tx.walletIds.empty()) {
         SPDLOG_LOGGER_ERROR(thisPtr->logger_, "invalid unsigned TX");
         BSMessageBox(BSMessageBox::critical, tr("Transaction import")
            , tr("Import failed"), tr("Invalid unsigned TX (corrupted file)")).exec();
         return;
      }
      std::string selectedWalletId;
      unsigned int foundWallets = 0;
      for (const auto &walletId : tx.walletIds) {
#if 0
         if (thisPtr->walletsManager_->getWalletById(walletId)) {
            if (selectedWalletId.empty()) {
               selectedWalletId = walletId;
            }
            foundWallets++;
         }
#endif
      }

      ui_->textEditComment->insertPlainText(QString::fromStdString(tx.comment));
      thisPtr->transactionData_->setFixedInputs(tx.getInputs(nullptr));

      if (selectedWalletId.empty()) {
         thisPtr->usedInputsModel_->updateInputs(tx.getInputs(nullptr));
         thisPtr->ui_->comboBoxWallets->hide();
         thisPtr->ui_->labelWallet->hide();
      }
      else {
         auto cbInputsReceived = [thisPtr, inputs = tx.getInputs(nullptr)]{
            if (!thisPtr) {
               return;
            }
            thisPtr->usedInputsModel_->updateInputs(inputs);
         };
         thisPtr->ui_->comboBoxWallets->show();
         thisPtr->ui_->labelWallet->show();
         SetFixedWallet(selectedWalletId, cbInputsReceived);
      }
      thisPtr->ui_->labelBalance->hide();
      thisPtr->ui_->labelBal->hide();

      const auto summary = thisPtr->transactionData_->GetTransactionSummary();
      importedTxTotalFee_ = bs::XBTAmount((int64_t)tx.fee);

      if (tx.change.value) {
         SetFixedChangeAddress(QString::fromStdString(tx.change.address.display()));
      }
      SetPredefinedFee(tx.fee);
      labelEstimatedFee()->setText(UiUtils::displayAmount(tx.fee));

      std::vector<Recipient> recipients;
      for (unsigned i=0; i<tx.armorySigner_.getTxOutCount(); i++) {
         auto recip = tx.armorySigner_.getRecipient(i);
         const auto addr = bs::Address::fromRecipient(recip);
         recipients.push_back({ addr, bs::XBTAmount{(int64_t)recip->getValue()}, false });
      }
      AddRecipients(recipients);

      broadcasting_ = !tx.isValid() || (foundWallets < tx.walletIds.size());
   }

   ui_->checkBoxRBF->setChecked(tx.RBF);
   ui_->checkBoxRBF->setEnabled(false);

   if (HaveSignedImportedTransaction()) {
      disableOutputsEditing();
      disableInputSelection();
      enableFeeChanging(false);
      feeChangeDisabled_ = true;
      disableChangeAddressSelecting();
   }
   updateCreateButtonText();
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

void CreateTransactionDialogAdvanced::onAddresses(const std::string &walletId
   , const std::vector<bs::sync::Address>& addrs)
{
   if (selChangeAddrDlg_) {
      selChangeAddrDlg_->onAddresses(walletId, addrs);
   }
}

void CreateTransactionDialogAdvanced::onAddressComments(const std::string& walletId
   , const std::map<bs::Address, std::string>& addrComments)
{
   if (selChangeAddrDlg_) {
      selChangeAddrDlg_->onAddressComments(walletId, addrComments);
   }
}

void CreateTransactionDialogAdvanced::onAddressBalances(const std::string& walletId
   , const std::vector<bs::sync::WalletBalanceData::AddressBalance>& addrBal)
{
   if (selChangeAddrDlg_) {
      selChangeAddrDlg_->onAddressBalances(walletId, addrBal);
   }
}

void CreateTransactionDialogAdvanced::onExistingAddressSelectedForChange()
{
   selChangeAddrDlg_ = new SelectAddressDialog(this, AddressListModel::AddressType::Internal);
   connect(selChangeAddrDlg_, &SelectAddressDialog::needExtAddresses, this, &CreateTransactionDialog::needExtAddresses);
   connect(selChangeAddrDlg_, &SelectAddressDialog::needIntAddresses, this, &CreateTransactionDialog::needIntAddresses);
   connect(selChangeAddrDlg_, &SelectAddressDialog::needUsedAddresses, this, &CreateTransactionDialog::needUsedAddresses);
   connect(selChangeAddrDlg_, &SelectAddressDialog::needAddrComments, this, &CreateTransactionDialog::needAddrComments);

   std::vector<bs::sync::WalletInfo> wallets;
   std::unordered_set<std::string> balanceIds;
   for (const auto& walletId : transactionData_->getWallets()) {
      const auto& itHdWallet = hdWallets_.find(walletId);
      if (itHdWallet != hdWallets_.end()) {
         for (const auto& group : itHdWallet->second.groups) {
            for (const auto& leaf : group.leaves) {
               bs::sync::WalletInfo wi;
               wi.format = bs::sync::WalletFormat::Plain;
               wi.ids = leaf.ids;
               if (leaf.ids.size() == 2) {
                  balanceIds.insert(leaf.ids.at(1));
               }
               else {
                  balanceIds.insert(leaf.ids.at(0));
               }
               wi.name = leaf.name;
               wi.type = bs::core::wallet::Type::Bitcoin;
               wi.primary = itHdWallet->second.primary;
               wallets.push_back(wi);
            }
         }
      }
   }
   selChangeAddrDlg_->setWallets(wallets);
   for (const auto& walletId : balanceIds) {
      emit needWalletBalances(walletId);
   }

   if (selChangeAddrDlg_->exec() == QDialog::Accepted) {
      selectedChangeAddress_ = selChangeAddrDlg_->getSelectedAddress();
      showExistingChangeAddress(true);
   } else {
      if (!selectedChangeAddress_.isValid()) {
         ui_->radioButtonNewAddrNative->setChecked(true);
      }
   }
   if (selChangeAddrDlg_) {
      selChangeAddrDlg_->deleteLater();
      selChangeAddrDlg_ = nullptr;
   }
}

void CreateTransactionDialogAdvanced::SetFixedWallet(const std::string& walletId, const std::function<void()> &cbInputsReset)
{
#if 0
   auto hdWallet = walletsManager_->getHDWalletById(walletId);
   auto walletType = UiUtils::WalletsTypes::None;
   if (!hdWallet) {
      hdWallet = walletsManager_->getHDRootForLeaf(walletId);
      if (hdWallet && (hdWallet->isHardwareWallet() || hdWallet->isHardwareOfflineWallet())) {
         for (auto leaf : hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves()) {
            if (leaf->walletId() == walletId) {
               walletType = UiUtils::getHwWalletType(leaf->purpose());
               break;
            }
         }
      }
   }

   const int idx = hdWallet ? SelectWallet(hdWallet->walletId(), walletType) : -1;
#else
   const int idx = -1;
#endif
   selectedWalletChanged(idx, true, cbInputsReset);
   ui_->comboBoxWallets->setEnabled(false);
}

void CreateTransactionDialogAdvanced::setFixedGroupInputs(const std::shared_ptr<bs::sync::hd::Group> &group
   , const std::vector<UTXO> &inputs)
{
   const auto leaves = group->getAllLeaves();
   if (leaves.empty()) {
      return;
   }
   SelectWallet(leaves.front()->walletId(), UiUtils::WalletsTypes::None);
   ui_->comboBoxWallets->setEnabled(false);
   disableInputSelection();
   transactionData_->setGroupAndInputs(group, inputs, topBlock_);
}

void CreateTransactionDialogAdvanced::disableOutputsEditing()
{
   ui_->lineEditAddress->setEnabled(false);
   ui_->lineEditAmount->setEnabled(false);
   ui_->pushButtonMax->setEnabled(false);
   ui_->pushButtonAddOutput->setEnabled(false);
   outputsModel_->enableRows(false);

   removeOutputEnabled_ = false;
}

void CreateTransactionDialogAdvanced::disableInputSelection()
{
   ui_->pushButtonSelectInputs->setEnabled(false);
   usedInputsModel_->enableRows(false);
}

void CreateTransactionDialogAdvanced::enableFeeChanging(bool enable)
{
   if (enable && feeChangeDisabled_) {
      return;
   }
   ui_->comboBoxFeeSuggestions->setEnabled(enable);
   ui_->doubleSpinBoxFeesManualPerByte->setEnabled(enable);
   ui_->spinBoxFeesManualTotal->setEnabled(enable);
}

void CreateTransactionDialogAdvanced::SetFixedChangeAddress(const QString& changeAddress)
{
   ui_->radioButtonExistingAddress->setChecked(true);
   ui_->radioButtonExistingAddress->setVisible(true);

   ui_->radioButtonNewAddrNative->setEnabled(false);
   ui_->radioButtonNewAddrNested->setEnabled(false);
   ui_->radioButtonNewAddrLegacy->setEnabled(false);
   ui_->radioButtonExistingAddress->setEnabled(false);

   ui_->radioButtonNewAddrNative->setVisible(false);
   ui_->radioButtonNewAddrNested->setVisible(false);
   ui_->radioButtonNewAddrLegacy->setVisible(false);

   selectedChangeAddress_ = bs::Address::fromAddressString(changeAddress.toStdString());
   showExistingChangeAddress(true);

   changeAddressFixed_ = true;
}

void CreateTransactionDialogAdvanced::SetPredefinedFee(const int64_t& manualFee)
{
   ui_->comboBoxFeeSuggestions->clear();
   ui_->comboBoxFeeSuggestions->addItem(tr("%1 satoshi").arg(manualFee), (qlonglong)manualFee);
   transactionData_->setTotalFee(manualFee);
}

void CreateTransactionDialogAdvanced::SetPredefinedFeeRate(const float feeRate)
{
   ui_->comboBoxFeeSuggestions->clear();
   ui_->comboBoxFeeSuggestions->addItem(tr("%1 s/b").arg(feeRate), (qlonglong)feeRate);
   transactionData_->setFeePerByte(feeRate);
}

// Set a TX such that it can't be altered.
void CreateTransactionDialogAdvanced::setUnchangeableTx()
{
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
      ui_->labelChangeAddress->setText(QString::fromStdString(selectedChangeAddress_.display()));
   } else {
      ui_->labelChangeAddress->clear();
   }
}

void CreateTransactionDialogAdvanced::disableChangeAddressSelecting()
{
   ui_->widgetChangeAddress->setEnabled(false);
}

void CreateTransactionDialogAdvanced::updateManualFeeControls()
{
   int itemIndex = ui_->comboBoxFeeSuggestions->currentIndex();
   int itemCount = ui_->comboBoxFeeSuggestions->count();

   ui_->doubleSpinBoxFeesManualPerByte->setVisible(itemCount >= 2 && itemIndex == itemCount - 2);

   const bool totalFeeSelected = (itemCount >= 2) && (itemIndex == itemCount - 1);
   ui_->spinBoxFeesManualTotal->setVisible(totalFeeSelected);
   if (totalFeeSelected) {
      ui_->spinBoxFeesManualTotal->setValue((int)transactionData_->totalFee());
   }
}

void CreateTransactionDialogAdvanced::fixFeePerByte()
{
   const auto txVirtSize = transactionData_->GetTransactionSummary().txVirtSize;
   if (!txVirtSize) {
      return;
   }

   // Any time the TX is adjusted under RBF, the minimum fee is adjusted.
   if (isRBF_) {
      const uint64_t newMinFee = originalFee_ + txVirtSize;
      SetMinimumFee(newMinFee, originalFeePerByte_);
   }

   // If the new fee is less than the minimum required fee, increase the fee.
   const auto totalFee = txVirtSize * transactionData_->feePerByte();
   if ((minTotalFee_ > 0) && (totalFee > 0) && (totalFee <= minTotalFee_)) {
      const float newFPB = (minTotalFee_ + 1) / txVirtSize;
      if (std::abs(transactionData_->feePerByte() - newFPB) > 0.01) {
         transactionData_->setFeePerByte(newFPB);
         if (ui_->comboBoxFeeSuggestions->currentIndex() == (ui_->comboBoxFeeSuggestions->count() - 2)) {
            QMetaObject::invokeMethod(this, [this, newFPB] {
               ui_->doubleSpinBoxFeesManualPerByte->setValue(newFPB);
            });
         }
      }
   }
}

void CreateTransactionDialogAdvanced::setTxFees()
{
   const int itemIndex = ui_->comboBoxFeeSuggestions->currentIndex();
   const int itemCount = ui_->comboBoxFeeSuggestions->count();

   if (itemIndex < (ui_->comboBoxFeeSuggestions->count() - 2)) {
      CreateTransactionDialog::feeSelectionChanged(itemIndex);
   } else if (itemIndex == itemCount - 2) {
      transactionData_->setFeePerByte(float(ui_->doubleSpinBoxFeesManualPerByte->value()));
      fixFeePerByte();
   } else if (itemIndex == itemCount - 1) {
      uint64_t fee = ui_->spinBoxFeesManualTotal->value();
      if ((minTotalFee_ > 0) && (fee < minTotalFee_)) {
         fee = minTotalFee_;
      }
      transactionData_->setTotalFee(fee, false);
   }

   validateAddOutputButton();

   if (FixRecipientsAmount()) {
      ui_->comboBoxFeeSuggestions->setCurrentIndex(itemCount - 1);
      ui_->doubleSpinBoxFeesManualPerByte->setValue(minFeePerByte_);
      ui_->doubleSpinBoxFeesManualPerByte->setVisible(false);
      ui_->spinBoxFeesManualTotal->setVisible(true);
      ui_->spinBoxFeesManualTotal->setValue(transactionData_->GetTransactionSummary().totalFee);
      enableFeeChanging(false);
   }
}


QLabel* CreateTransactionDialogAdvanced::labelTXAmount() const
{
   return ui_->labelTransactionAmount;
}

QLabel* CreateTransactionDialogAdvanced::labelTxOutputs() const
{
   return ui_->labelTXOutputs;
}

void CreateTransactionDialogAdvanced::onSimpleDialogRequested()
{
   simpleDialogRequested_ = true;
   accept();
}

bool CreateTransactionDialogAdvanced::switchModeRequested() const
{
   return simpleDialogRequested_;
}

std::shared_ptr<CreateTransactionDialog> CreateTransactionDialogAdvanced::SwitchMode()
{
   if (!paymentInfo_.address.isEmpty()) {
      return CreateTransactionDialogSimple::CreateForPaymentRequest(topBlock_
         , logger_, paymentInfo_, parentWidget());
   }

   auto simpleDialog = std::make_shared<CreateTransactionDialogSimple>(topBlock_
      , logger_, parentWidget());

   simpleDialog->SelectWallet(UiUtils::getSelectedWalletId(ui_->comboBoxWallets, ui_->comboBoxWallets->currentIndex()),
      UiUtils::getSelectedWalletType(ui_->comboBoxWallets));

   const auto recipientIdList = transactionData_->allRecipientIds();

   if (recipientIdList.size() <= 1) {
      if (recipientIdList.empty()) {
         // try to add details from UI
         auto address = ui_->lineEditAddress->text().trimmed();
         if (!address.isEmpty()) {
            simpleDialog->preSetAddress(address);
         }

         auto valueText = ui_->lineEditAmount->text();
         if (!valueText.isEmpty()) {
            double value = UiUtils::parseAmountBtc(valueText);
            simpleDialog->preSetValue(value);
         }
      }
      else {
         // set details from first recipient
         simpleDialog->preSetAddress(QString::fromStdString(transactionData_->GetRecipientAddress(recipientIdList[0]).display()));
         simpleDialog->preSetValue(transactionData_->GetRecipientAmount(recipientIdList[0]).GetValueBitcoin());
      }
   }

   return simpleDialog;
}

void CreateTransactionDialogAdvanced::validateAmountLine()
{
   setValidationStateOnAmount(isCurrentAmountValid());
}
