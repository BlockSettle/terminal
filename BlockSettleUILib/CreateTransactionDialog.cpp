/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CreateTransactionDialog.h"

#include <stdexcept>
#include <thread>

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>

#include <spdlog/spdlog.h>

#include "Address.h"
#include "ArmoryConnection.h"
#include "BSMessageBox.h"
#include "OfflineSigner.h"
#include "SelectedTransactionInputs.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "UsedInputsModel.h"
#include "UtxoReservationManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "XbtAmountValidator.h"

// Mirror of cached Armory wait times - NodeRPC::aggregateFeeEstimates()
const std::map<unsigned int, QString> kFeeLevels = {
   { 2, QObject::tr("20 minutes") },
   { 4, QObject::tr("40 minutes") },
   { 6, QObject::tr("1 hour") },
   { 12, QObject::tr("2 hours") },
   { 24, QObject::tr("4 hours") },
   { 48, QObject::tr("8 hours") },
   { 144, QObject::tr("24 hours") },
};
const size_t kTransactionWeightLimit = 400000;

CreateTransactionDialog::CreateTransactionDialog(bool loadFeeSuggestions
   , uint32_t topBlock, const std::shared_ptr<spdlog::logger>& logger
   , QWidget* parent)
   : QDialog(parent)
   , logger_(logger)
   , loadFeeSuggestions_(loadFeeSuggestions)
   , topBlock_(topBlock)
{
   qRegisterMetaType<std::map<unsigned int, float>>();
}

CreateTransactionDialog::~CreateTransactionDialog() noexcept = default;

void CreateTransactionDialog::init()
{
   if (!transactionData_) {
      transactionData_ = std::make_shared<TransactionData>([this]() {
         QMetaObject::invokeMethod(this, [this] {
            onTransactionUpdated();
         });
      }, logger_);
   }
   else {
      const auto recipients = transactionData_->allRecipientIds();
      for (const auto &recipId : recipients) {
         transactionData_->RemoveRecipient(recipId);
      }
      onTransactionUpdated();
      transactionData_->SetCallback([this] {
         QMetaObject::invokeMethod(this, [this] {
            // Call on main thread because GUI is updated here
            onTransactionUpdated();
         });
      });
   }

   xbtValidator_ = new XbtAmountValidator(this);
   lineEditAmount()->setValidator(xbtValidator_);

   populateWalletsList();
   if (loadFeeSuggestions_) {
      populateFeeList();
   }

   pushButtonMax()->setEnabled(false);
   checkBoxRBF()->setChecked(true);
   pushButtonCreate()->setEnabled(false);

   connect(pushButtonMax(), &QPushButton::clicked, this, &CreateTransactionDialog::onMaxPressed);
   connect(comboBoxFeeSuggestions(), SIGNAL(activated(int)), this, SLOT(feeSelectionChanged(int)));
   connect(comboBoxWallets(), SIGNAL(currentIndexChanged(int)), this, SLOT(selectedWalletChanged(int)));

   updateCreateButtonText();
   lineEditAddress()->setFocus();
}

void CreateTransactionDialog::updateCreateButtonText()
{
   if (HaveSignedImportedTransaction()) {
      pushButtonCreate()->setText(tr("Broadcast"));
      if (signContainer_->isOffline()) {
         pushButtonCreate()->setEnabled(false);
      }
      return;
   }
   const auto walletId = UiUtils::getSelectedWalletId(comboBoxWallets());

   if (walletsManager_) {
      auto walletPtr = walletsManager_->getHDWalletById(walletId);
      if (walletPtr && !walletPtr->isHardwareWallet() && (signContainer_->isOffline()
         || signContainer_->isWalletOffline(walletId))) {
         pushButtonCreate()->setText(tr("Export"));
      } else {
         selectedWalletChanged(-1);
      }
   }
}

void CreateTransactionDialog::onSignerAuthenticated()
{
   selectedWalletChanged(-1);
}

void CreateTransactionDialog::clear()
{
   transactionData_->clear();
   importedSignedTX_.clear();
   lineEditAddress()->clear();
   lineEditAmount()->clear();
   textEditComment()->clear();
}

void CreateTransactionDialog::reject()
{
   if (broadcasting_) {
      BSMessageBox confirmExit(BSMessageBox::question, tr("Abort transaction broadcast")
         , tr("Abort transaction broadcast?")
         , tr("Are you sure you wish to abort the signing and broadcast process?"), this);
      //confirmExit.setExclamationIcon();
      if (confirmExit.exec() != QDialog::Accepted) {
         return;
      }

      if (txReq_.isValid() && signContainer_) {
         signContainer_->CancelSignTx(BinaryData::fromString(txReq_.serializeState().SerializeAsString()));
      }
   }

   QDialog::reject();
}

void CreateTransactionDialog::closeEvent(QCloseEvent *e)
{
   reject();

   e->ignore();
}

int CreateTransactionDialog::SelectWallet(const std::string& walletId, UiUtils::WalletsTypes type)
{
   auto index = UiUtils::selectWalletInCombobox(comboBoxWallets(), walletId
      , static_cast<UiUtils::WalletsTypes>(type));
   if (index < 0) {
      if (walletsManager_) {
         const auto rootWallet = walletsManager_->getHDRootForLeaf(walletId);
         if (rootWallet) {
            index = UiUtils::selectWalletInCombobox(comboBoxWallets(), rootWallet->walletId());
         }
      }
      else {
         //TODO: select root wallet if needed
      }
   }
   return index;
}

void CreateTransactionDialog::populateWalletsList()
{
   if (walletsManager_) {
      int index = UiUtils::fillHDWalletsComboBox(comboBoxWallets(), walletsManager_, UiUtils::WalletsTypes::All_AllowHwLegacy);
      selectedWalletChanged(index);
   }
   else {
      emit needWalletsList(UiUtils::WalletsTypes::All_AllowHwLegacy, "CreateTX");
   }
}

void CreateTransactionDialog::onWalletsList(const std::string &id, const std::vector<bs::sync::HDWalletData>& hdWallets)
{
   if (id != "CreateTX") {
      return;
   }
   int selected = 0;
   auto comboBox = comboBoxWallets();
   comboBox->clear();

   const auto &addRow = [comboBox]
      (const std::string& label, const std::string& walletId, UiUtils::WalletsTypes type)
   {
      int i = comboBox->count();
      comboBox->addItem(QString::fromStdString(label));
      comboBox->setItemData(i, QString::fromStdString(walletId), UiUtils::WalletIdRole);
      comboBox->setItemData(i, QVariant::fromValue(static_cast<int>(type)), UiUtils::WalletType);
   };

   hdWallets_.clear();
   for (const auto& hdWallet : hdWallets) {
      hdWallets_[hdWallet.id] = hdWallet;
      if (hdWallet.primary) {
         selected = comboBox->count();
      }
      UiUtils::WalletsTypes type = UiUtils::WalletsTypes::None;
      if ((hdWallet.groups.size() == 1) && (hdWallet.groups[0].leaves.size() == 1)) {
         const auto& leaf = hdWallet.groups[0].leaves.at(0);
         std::string label = hdWallet.name;
         const auto purpose = static_cast<bs::hd::Purpose>(leaf.path.get(0) & ~bs::hd::hardFlag);
         if (purpose == bs::hd::Purpose::Native) {
            label += " Native";
            type = UiUtils::WalletsTypes::HardwareNativeSW;
         } else if (purpose == bs::hd::Purpose::Nested) {
            label += " Nested";
            type = UiUtils::WalletsTypes::HardwareNestedSW;
         } else if (purpose == bs::hd::Purpose::NonSegWit) {
            label += " Legacy";
            type = UiUtils::WalletsTypes::HardwareLegacy;
         }
         addRow(label, hdWallet.id, type);
      }
      else {
         if (hdWallet.offline) {
            type = UiUtils::WalletsTypes::WatchOnly;
         } else {
            type = UiUtils::WalletsTypes::Full;
         }
         addRow(hdWallet.name, hdWallet.id, type);
      }
   }
   comboBox->setCurrentIndex(selected);
}

void CreateTransactionDialog::onFeeLevels(const std::map<unsigned int, float>& feeLevels)
{
   comboBoxFeeSuggestions()->clear();

   for (const auto& feeVal : feeLevels) {
      QString desc;
      const auto itLevel = kFeeLevels.find(feeVal.first);
      if (itLevel == kFeeLevels.end()) {
         desc = tr("%1 minutes").arg(10 * feeVal.first);
      } else {
         desc = itLevel->second;
      }
      comboBoxFeeSuggestions()->addItem(tr("%1 blocks (%2): %3 s/b").arg(feeVal.first).arg(desc).arg(feeVal.second)
         , feeVal.second);
   }

   comboBoxFeeSuggestions()->setEnabled(true);
   feeSelectionChanged(0);
}

void CreateTransactionDialog::populateFeeList()
{
   comboBoxFeeSuggestions()->addItem(tr("Loading Fee suggestions"));
   comboBoxFeeSuggestions()->setCurrentIndex(0);
   comboBoxFeeSuggestions()->setEnabled(false);

   if (walletsManager_) {
      connect(this, &CreateTransactionDialog::feeLoadingCompleted
         , this, &CreateTransactionDialog::onFeeSuggestionsLoaded
         , Qt::QueuedConnection);

      loadFees();
   }
   else {
      std::vector<unsigned int> feeLevels;
      feeLevels.reserve(kFeeLevels.size());
      for (const auto& level : kFeeLevels) {
         feeLevels.push_back(level.first);
      }
      emit needFeeLevels(feeLevels);
   }
}

void CreateTransactionDialog::loadFees()
{
   struct Result {
      std::map<unsigned int, float> values;
      std::set<unsigned int>  levels;
   };
   auto result = std::make_shared<Result>();

   for (const auto &feeLevel : kFeeLevels) {
      result->levels.insert(feeLevel.first);
   }
   for (const auto &feeLevel : kFeeLevels) {
      const auto &cbFee = [this, result, level=feeLevel.first](float fee) {
         result->levels.erase(level);
         if (fee < std::numeric_limits<float>::infinity()) {
            result->values[level] = fee;
         }
         if (result->levels.empty()) {
            emit feeLoadingCompleted(result->values);
         }
      };
      walletsManager_->estimatedFeePerByte(feeLevel.first, cbFee, this);
   }
}

void CreateTransactionDialog::onFeeSuggestionsLoaded(const std::map<unsigned int, float> &feeValues)
{
   comboBoxFeeSuggestions()->clear();

   for (const auto &feeVal : feeValues) {
      QString desc;
      const auto itLevel = kFeeLevels.find(feeVal.first);
      if (itLevel == kFeeLevels.end()) {
         desc = tr("%1 minutes").arg(10 * feeVal.first);
      }
      else {
         desc = itLevel->second;
      }
      comboBoxFeeSuggestions()->addItem(tr("%1 blocks (%2): %3 s/b").arg(feeVal.first).arg(desc).arg(feeVal.second)
         , feeVal.second);
   }

   comboBoxFeeSuggestions()->setEnabled(true);

   feeSelectionChanged(0);
}

void CreateTransactionDialog::feeSelectionChanged(int currentIndex)
{
   transactionData_->setFeePerByte(comboBoxFeeSuggestions()->itemData(currentIndex).toFloat());
}

void CreateTransactionDialog::selectedWalletChanged(int, bool resetInputs, const std::function<void()> &cbInputsReset)
{
   if (!comboBoxWallets()->count()) {
      pushButtonCreate()->setText(tr("No wallets"));
      return;
   }
   const auto walletId = UiUtils::getSelectedWalletId(comboBoxWallets());
   if (walletsManager_) {
      const auto rootWallet = walletsManager_->getHDWalletById(walletId);
      if (!rootWallet) {
         logger_->error("[{}] wallet with id {} not found", __func__, walletId);
         return;
      }
      if (!rootWallet->isHardwareWallet() && (signContainer_->isWalletOffline(rootWallet->walletId())
         || !rootWallet || signContainer_->isWalletOffline(rootWallet->walletId()))) {
         pushButtonCreate()->setText(tr("Export"));
      } else {
         pushButtonCreate()->setText(tr("Broadcast"));
      }

      auto group = rootWallet->getGroup(rootWallet->getXBTGroupType());
      const bool isHardware = rootWallet->isHardwareWallet() || rootWallet->isHardwareOfflineWallet();
      bs::hd::Purpose hwPurpose;
      if (isHardware) {
         hwPurpose = UiUtils::getSelectedHwPurpose(comboBoxWallets());
      }

      if (transactionData_->getGroup() != group || isHardware || resetInputs) {
         if (isHardware) {
            transactionData_->setWallet(group->getLeaf(hwPurpose), armory_->topBlock()
               , resetInputs, cbInputsReset);
         } else {
            transactionData_->setGroup(group, armory_->topBlock(), false
               , resetInputs, cbInputsReset);
         }
      }
   }
   else {
      if (walletId.empty()) {
         return;
      }
      pushButtonCreate()->setText(tr("Broadcast"));
      emit needUTXOs("CreateTX", walletId);
   }

   emit walletChanged();
}

void CreateTransactionDialog::onUTXOs(const std::string& id
   , const std::string& walletId, const std::vector<UTXO>& utxos)
{
   if (id != "CreateTX") {
      return;
   }
   logger_->debug("[{}] {}", __func__, walletId);
   transactionData_->setUTXOs({ walletId }, topBlock_, utxos);
}

void CreateTransactionDialog::onTransactionUpdated()
{
   const auto &summary = transactionData_->GetTransactionSummary();

   // #UTXO_MANAGER: reserve all available UTXO for now till the moment dialog will be deleted.
   // Used to prevent UTXO conflicts with AQ script.
   // FIXME: Disabled as it caused problems when advanced dialog selected (available balance becomes 0 and list of available UTXOs is empty until different wallet selected).
#if 0
   if (summary.availableBalance > .0) {
      utxoRes_.release();
      utxoRes_ = utxoReservationManager_->makeNewReservation(transactionData_->getSelectedInputs()->GetAllTransactions());
   }
#endif
   labelBalance()->setText(UiUtils::displayAmount(summary.availableBalance));
   labelAmount()->setText(UiUtils::displayAmount(summary.selectedBalance));
   labelTxInputs()->setText(summary.isAutoSelected ? tr("Auto (%1)").arg(QString::number(summary.usedTransactions))
      : QString::number(summary.usedTransactions));
   labelTxOutputs()->setText(QString::number(summary.outputsCount));
   labelEstimatedFee()->setText(UiUtils::displayAmount(summary.totalFee));
   labelTotalAmount()->setText(UiUtils::displayAmount(summary.balanceToSpend + UiUtils::amountToBtc(summary.totalFee)));
   labelTXAmount()->setText(UiUtils::displayAmount(summary.balanceToSpend));
   if (labelTxSize()) {
      labelTxSize()->setText(QString::number(summary.txVirtSize));
   }

   if (feePerByteLabel() != nullptr) {
      feePerByteLabel()->setText(tr("%1 s/b").arg(summary.feePerByte));
   }

   if (changeLabel() != nullptr) {
      if (summary.hasChange) {
         changeLabel()->setText(UiUtils::displayAmount(summary.selectedBalance - summary.balanceToSpend - UiUtils::amountToBtc(summary.totalFee)));
      } else {
         changeLabel()->setText(UiUtils::displayAmount(0.0));
      }
   }

   pushButtonCreate()->setEnabled(transactionData_->IsTransactionValid());
}

void CreateTransactionDialog::onMaxPressed()
{
   pushButtonMax()->setEnabled(false);
   lineEditAmount()->setEnabled(false);
   QCoreApplication::processEvents();

   const auto outputAddr = bs::Address::fromAddressString(lineEditAddress()->text().trimmed().toStdString());
   const auto maxValue = transactionData_->CalculateMaxAmount(outputAddr);
   if (maxValue > 0) {
      lineEditAmount()->setText(UiUtils::displayAmount(maxValue));
   }
   pushButtonMax()->setEnabled(true);
   lineEditAmount()->setEnabled(true);
}

void CreateTransactionDialog::onSignedTX(const std::string& id, BinaryData signedTX
   , bs::error::ErrorCode result)
{
   if (id != "CreateTX") {
      return;
   }
   if (result == bs::error::ErrorCode::TxCancelled) {
      stopBroadcasting();
      return;
   }

   BinaryData txHash;
   try {
      if (result != bs::error::ErrorCode::NoError) {
         throw std::runtime_error(bs::error::ErrorCodeToString(result).toStdString());
      }

      if (signedTX.empty()) {
         throw std::runtime_error("Empty signed TX data received");
      }
      const Tx tx(signedTX);
      txHash = tx.getThisHash();
      if (tx.isInitialized() && (tx.getTxWeight() >= kTransactionWeightLimit)) {
         BSMessageBox mBox(BSMessageBox::question, tr("Oversized Transaction")
            , tr("Transaction size limit %1 exceeded: %2. Do you still want to send this transaction?")
            .arg(QString::number(kTransactionWeightLimit)).arg(QString::number(tx.getTxWeight()))
            , this);
         if (mBox.exec() != QDialog::Accepted) {
            stopBroadcasting();
            return;
         }
      }
   } catch (const std::exception& e) {
      MessageBoxBroadcastError(tr("Invalid signed transaction: %1").arg(QLatin1String(e.what()))
         , result, this).exec();
      stopBroadcasting();
      return;
   }

   QString detailedText;
   if (result == bs::error::ErrorCode::NoError) {
      emit needBroadcastZC("CreateTX", signedTX);
      if (!textEditComment()->document()->isEmpty()) {
         const auto& comment = textEditComment()->document()->toPlainText().toStdString();
         emit needSetTxComment(transactionData_->getWallets().at(0), txHash, comment);
      }
      accept();
      return;
   } else {
      detailedText = bs::error::ErrorCodeToString(result);
   }
   MessageBoxBroadcastError(detailedText, result, this).exec();
   stopBroadcasting();
}

void CreateTransactionDialog::onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result)
{
   if (!pendingTXSignId_ || (pendingTXSignId_ != id)) {
      return;
   }

   pendingTXSignId_ = 0;
   QString detailedText;

   if (result == bs::error::ErrorCode::TxCancelled) {
      stopBroadcasting();
      return;
   }

   try {
      if (result != bs::error::ErrorCode::NoError) {
         throw std::runtime_error(bs::error::ErrorCodeToString(result).toStdString());
      }

      if (signedTX.empty()) {
         throw std::runtime_error("Empty signed TX data received");
      }
      const Tx tx(signedTX);
      if (tx.isInitialized() && (tx.getTxWeight() >= kTransactionWeightLimit)) {
         BSMessageBox mBox(BSMessageBox::question, tr("Oversized Transaction")
            , tr("Transaction size limit %1 exceeded: %2. Do you still want to send this transaction?")
            .arg(QString::number(kTransactionWeightLimit)).arg(QString::number(tx.getTxWeight()))
            , this);
         if (mBox.exec() != QDialog::Accepted) {
            stopBroadcasting();
            return;
         }
      }
   }
   catch (const std::exception &e) {
      MessageBoxBroadcastError(tr("Invalid signed transaction: %1").arg(QLatin1String(e.what()))
         , result, this).exec();
      stopBroadcasting();
      return;
   }

   if (result == bs::error::ErrorCode::NoError) {
      if (!armory_->broadcastZC(signedTX).empty()) {
         if (!textEditComment()->document()->isEmpty()) {
            const auto &comment = textEditComment()->document()->toPlainText().toStdString();
            transactionData_->getWallet()->setTransactionComment(signedTX, comment);
         }
         accept();
         return;
      }

      detailedText = tr("Failed to communicate to BlockSettleDB to broadcast transaction. Maybe BlockSettleDB is offline");
   }
   else {
      detailedText = bs::error::ErrorCodeToString(result);
   }

   MessageBoxBroadcastError(detailedText, result, this).exec();

   stopBroadcasting();
}

void CreateTransactionDialog::startBroadcasting()
{
   broadcasting_ = true;
   QMetaObject::invokeMethod(this, [this] {
      pushButtonCreate()->setEnabled(false);
      pushButtonCreate()->setText(tr("Waiting for TX signing..."));
   });
}

void CreateTransactionDialog::stopBroadcasting()
{
   broadcasting_ = false;
   QMetaObject::invokeMethod(this, [this] {
      pushButtonCreate()->setEnabled(true);
      updateCreateButtonText();
   });
}

bool CreateTransactionDialog::BroadcastImportedTx()
{
   if (importedSignedTX_.empty()) {
      return false;
   }
   startBroadcasting();
   if (!armory_->broadcastZC(importedSignedTX_).empty()) {
      if (!textEditComment()->document()->isEmpty()) {
         const auto &comment = textEditComment()->document()->toPlainText().toStdString();
         transactionData_->getWallet()->setTransactionComment(importedSignedTX_, comment);
      }
      return true;
   }
   importedSignedTX_.clear();
   stopBroadcasting();
   BSMessageBox(BSMessageBox::critical, tr("Transaction broadcast"), tr("Failed to broadcast imported transaction"), this).exec();
   return false;
}

void CreateTransactionDialog::CreateTransaction(const CreateTransactionCb &cb)
{
   if (signContainer_ && walletsManager_) {  // old code
/*      BSMessageBox(BSMessageBox::critical, tr("Error")
         , tr("Signer is invalid - unable to send transaction"), this).exec();
      return;*/
      getChangeAddress([this, cb = std::move(cb), handle = validityFlag_.handle()](bs::Address changeAddress) {
         if (!handle.isValid()) {
            return;
         }
         try {
            txReq_ = transactionData_->createTXRequest(checkBoxRBF()->checkState() == Qt::Checked, changeAddress);
            if (!changeAddress.empty()) {
               auto changeWallet = walletsManager_->getWalletByAddress(changeAddress);
               assert(changeWallet);
               if (std::find(txReq_.walletIds.begin(), txReq_.walletIds.end(), changeWallet->walletId()) == txReq_.walletIds.end()) {
                  txReq_.walletIds.push_back(changeWallet->walletId());
               }
            }

            // grab supporting transactions for the utxo map.
            // required only for HW
            std::set<BinaryData> hashes;
            for (unsigned i=0; i<txReq_.armorySigner_.getTxInCount(); i++) {
               auto spender = txReq_.armorySigner_.getSpender(i);
               hashes.emplace(spender->getOutputHash());
            }

            auto supportingTxMapCb = [this, handle, cb]
                  (const AsyncClient::TxBatchResult& result, std::exception_ptr eptr) mutable
            {
               if (!handle.isValid()) {
                  return;
               }

               if (eptr) {
                  SPDLOG_LOGGER_ERROR(logger_, "getTXsByHash failed");
                  cb(false, "receving supporting transactions failed", "", 0);
                  return;
               }

               for (auto& txPair : result) {
                  txReq_.armorySigner_.addSupportingTx(*txPair.second);
               }

               const auto cbResolvePublicData = [this, handle, cb]
                     (bs::error::ErrorCode result, const Codec_SignerState::SignerState &state)
               {
                  txReq_.armorySigner_.deserializeState(state);
                  if (!handle.isValid()) {
                     return;
                  }

                  logger_->debug("[CreateTransactionDialog::CreateTransaction cbResolvePublicData] result={}, state: {}"
                                 , (int)result, state.IsInitialized());

                  const auto serializedUnsigned = txReq_.armorySigner_.serializeUnsignedTx().toHexStr();
                  const auto estimatedSize = txReq_.estimateTxVirtSize();

                  cb(true, "", serializedUnsigned, estimatedSize);
               };

               signContainer_->resolvePublicSpenders(txReq_, cbResolvePublicData);
            };

            if (!armory_->getTXsByHash(hashes, supportingTxMapCb, true)) {
               SPDLOG_LOGGER_ERROR(logger_, "getTXsByHash failed");
               cb(false, "receving supporting transactions failed", "", 0);
            }
         }
         catch (const std::exception &e) {
            SPDLOG_LOGGER_ERROR(logger_, "exception: {}", e.what());
            cb(false, e.what(), "", 0);
         }
      });
      return;
   }

   getChangeAddress([this, cb](const bs::Address &changeAddress) {
      try {
         txReq_ = transactionData_->createTXRequest(checkBoxRBF()->checkState() == Qt::Checked, changeAddress);

         // grab supporting transactions for the utxo map.
         // required only for HW
         std::set<BinaryData> hashes;
         for (unsigned i = 0; i < txReq_.armorySigner_.getTxInCount(); i++) {
            auto spender = txReq_.armorySigner_.getSpender(i);
            hashes.emplace(spender->getOutputHash());
         }
         //TODO: implement supporting TXs collection

         const auto serializedUnsigned = txReq_.armorySigner_.serializeUnsignedTx().toHexStr();
         const auto estimatedSize = txReq_.estimateTxVirtSize();
         cb(true, "", serializedUnsigned, estimatedSize);
      } catch (const std::exception& e) {
         SPDLOG_LOGGER_ERROR(logger_, "exception: {}", e.what());
         cb(false, e.what(), {}, 0);
      }
   });
}

bool CreateTransactionDialog::createTransactionImpl()
{
   QString text;
   QString detailedText;

   try {
      txReq_.comment = textEditComment()->document()->toPlainText().toStdString();

      if (isRBF_) {
         // We shouldn't hit this case since the request checks the incremental
         // relay fee requirement for RBF. But, in case we
         if (txReq_.fee <= originalFee_) {
            BSMessageBox(BSMessageBox::info, tr("Warning"), tr("Fee is too low"),
               tr("Due to RBF requirements, the current fee (%1) will be " \
                  "increased 1 satoshi above the original transaction fee (%2)")
               .arg(UiUtils::displayAmount(txReq_.fee))
               .arg(UiUtils::displayAmount(originalFee_))).exec();
            txReq_.fee = originalFee_ + 1;
         }

         const float newFeePerByte = transactionData_->feePerByte();
         if ((originalFeePerByte_ - newFeePerByte) > 0.005) {  // allow some rounding
            BSMessageBox(BSMessageBox::info, tr("Warning"), tr("Fee per byte is too low"),
               tr("Due to RBF requirements, the current fee per byte (%1) will " \
                  "be increased to the original transaction fee rate (%2)")
               .arg(newFeePerByte)
               .arg(originalFeePerByte_)).exec();
            txReq_.fee = std::ceil(txReq_.fee * (originalFeePerByte_ / newFeePerByte));
         }
      }
      else if (isCPFP_) {
         const auto title = tr("CPFP fee warning");
         if (txReq_.fee < originalFee_ + addedFee_) {
            txReq_.fee = originalFee_ + addedFee_;
            BSMessageBox(BSMessageBox::info, tr("Warning"), title,
               tr("In order to ensure the CPFP transaction gets mined within the next two blocks, "
                  "we recommend the total fee to be no less than %1")
               .arg(UiUtils::displayAmount(txReq_.fee))).exec();
         }

         const float newFeePerByte = transactionData_->feePerByte();
         if ((advisedFeePerByte_ - newFeePerByte) > 0.005) {  // allow some rounding
            const auto prevFee = txReq_.fee;
            txReq_.fee = std::ceil(txReq_.fee * (advisedFeePerByte_ / newFeePerByte));
            BSMessageBox(BSMessageBox::info, tr("Warning"), title,
               tr("In order to ensure the CPFP transaction gets mined within the next two blocks, "
                  "we recommend the fee per byte to be no less than %1")
               .arg(advisedFeePerByte_)).exec();
         }
      }
      else {
         // do we need some checks here?
      }

      if (!txReq_.armorySigner_.hasLegacyInputs()) {
         txReq_.txHash = txReq_.txId();
      }

      if (walletsManager_ && signContainer_) {
         const auto hdWallet = walletsManager_->getHDWalletById(UiUtils::getSelectedWalletId(comboBoxWallets()));
         if (hdWallet->isOffline() && !hdWallet->isHardwareWallet()) {
            QString offlineFilePath;
            QString signerOfflineDir = applicationSettings_->get<QString>(ApplicationSettings::signerOfflineDir);

            const qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
            const std::string fileName = fmt::format("{}_{}.bin", hdWallet->walletId(), timestamp);

            QString defaultFilePath = QDir(signerOfflineDir).filePath(QString::fromStdString(fileName));
            offlineFilePath = QFileDialog::getSaveFileName(this, tr("Save Offline TX as...")
               , defaultFilePath, tr("TX files (*.bin);; All files (*)"));

            if (offlineFilePath.isEmpty()) {
               return true;
            }

            QFileInfo exportFileIndo(offlineFilePath);
            QString newSignerOfflineDir = exportFileIndo.absoluteDir().path();
            if (signerOfflineDir != newSignerOfflineDir) {
               applicationSettings_->set(ApplicationSettings::signerOfflineDir, newSignerOfflineDir);
            }
            if (exportFileIndo.suffix() != QLatin1String("bin")) {
               offlineFilePath += QLatin1String(".bin");
            }

            bs::error::ErrorCode result = bs::core::wallet::ExportTxToFile(txReq_, offlineFilePath);
            if (result == bs::error::ErrorCode::NoError) {
               BSMessageBox(BSMessageBox::info, tr("Offline Transaction")
                  , tr("Request was successfully exported")
                  , tr("Saved to %1").arg(offlineFilePath), this).exec();
               return false; // export was success so we could close the dialog
            } else {
               BSMessageBox(BSMessageBox::warning, tr("Offline Transaction")
                  , tr("Failed to save offline Tx request")
                  , tr("Filename: %1").arg(offlineFilePath), this).exec();
            }
         } else {
            startBroadcasting();
            pendingTXSignId_ = signContainer_->signTXRequest(txReq_, SignContainer::TXSignMode::Full, true);
            if (!pendingTXSignId_) {
               throw std::logic_error("Signer failed to send request");
            }
         }
      }
      else {
         //TODO: add implementation for HW wallets
         startBroadcasting();
         emit needSignTX("CreateTX", txReq_, true);
      }
      return true;
   }
   catch (const std::runtime_error &e) {
      text = tr("Failed to broadcast transaction");
      detailedText = QString::fromStdString(e.what());
      SPDLOG_LOGGER_ERROR(logger_, "runtime exception: {}", e.what());
   }
   catch (const std::exception &e) {
      text = tr("Failed to create transaction");
      detailedText = QString::fromStdString(e.what());
      SPDLOG_LOGGER_ERROR(logger_, "exception: {}", e.what());
   }

   stopBroadcasting();
   BSMessageBox(BSMessageBox::critical, text, text, detailedText, this).exec();
   showError(text, detailedText);
   return false;
}

std::vector<bs::core::wallet::TXSignRequest> CreateTransactionDialog::ImportTransactions()
{
   QString signerOfflineDir = applicationSettings_->get<QString>(ApplicationSettings::signerOfflineDir);

   const QString reqFile = QFileDialog::getOpenFileName(this, tr("Select Transaction file"), signerOfflineDir
      , tr("TX files (*.bin);; All files (*)"));
   if (reqFile.isEmpty()) {
      return {};
   }

   // Update latest used directory if needed
   QString newSignerOfflineDir = QFileInfo(reqFile).absoluteDir().path();
   if (signerOfflineDir != newSignerOfflineDir) {
      applicationSettings_->set(ApplicationSettings::signerOfflineDir, newSignerOfflineDir);
   }

   const auto title = tr("Transaction file");
   QFile f(reqFile);
   if (!f.exists()) {
      showError(title, tr("File %1 doesn't exist").arg(reqFile));
      return {};
   }

   if (!f.open(QIODevice::ReadOnly)) {
      showError(title, tr("Failed to open %1 for reading").arg(reqFile));
      return {};
   }

   const auto data = f.readAll().toStdString();
   if (data.empty()) {
      showError(title, tr("File %1 contains no data").arg(reqFile));
      return {};
   }

   auto transactions = bs::core::wallet::ParseOfflineTXFile(data);
   if (transactions.size() != 1) {
      showError(title, tr("Invalid file %1 format").arg(reqFile));
      return {};
   }

   const auto envConf = static_cast<ApplicationSettings::EnvConfiguration>(applicationSettings_->get<int>(ApplicationSettings::envConfiguration));
   const bool isProd = (envConf == ApplicationSettings::EnvConfiguration::Production);
   const bool isTest = (envConf == ApplicationSettings::EnvConfiguration::Test);
   if ((isProd || isTest) && !transactions.at(0).allowBroadcasts) {
      BSMessageBox errorMessage(BSMessageBox::warning, tr("Warning"), tr("Import failure")
         , tr("You are trying to import a settlement transaction into a BlockSettle Terminal. "
              "Settlement transactions must be imported into a BlockSettle Signer if signed offline."), this);
      errorMessage.exec();
      return {};
   }

   clear();
   return transactions;
}

void CreateTransactionDialog::showError(const QString &text, const QString &detailedText)
{
   BSMessageBox errorMessage(BSMessageBox::critical, text, detailedText, this);
   errorMessage.exec();
}

bool CreateTransactionDialog::canUseSimpleMode(const Bip21::PaymentRequestInfo& paymentInfo)
{
   return paymentInfo.requestURL.isEmpty();
}
