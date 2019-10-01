#include "CreateTransactionDialog.h"

#include <stdexcept>
#include <thread>

#include <QDebug>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QIntValidator>
#include <QFile>
#include <QFileDialog>
#include <QCloseEvent>

#include "Address.h"
#include "ArmoryConnection.h"
#include "BSMessageBox.h"
#include "CoinControlDialog.h"
#include "OfflineSigner.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "TransactionOutputsModel.h"
#include "UiUtils.h"
#include "UsedInputsModel.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "XbtAmountValidator.h"

// Mirror of cached Armory wait times - NodeRPC::aggregateFeeEstimates()
const std::map<unsigned int, QString> feeLevels = {
   { 2, QObject::tr("20 minutes") },
   { 4, QObject::tr("40 minutes") },
   { 6, QObject::tr("1 hour") },
   { 12, QObject::tr("2 hours") },
   { 24, QObject::tr("4 hours") },
   { 48, QObject::tr("8 hours") },
   { 144, QObject::tr("24 hours") },
};
const size_t kTransactionWeightLimit = 400000;

CreateTransactionDialog::CreateTransactionDialog(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::sync::WalletsManager>& walletManager
   , const std::shared_ptr<SignContainer> &container, bool loadFeeSuggestions
   , const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ApplicationSettings> &applicationSettings
   , QWidget* parent)
   : QDialog(parent)
   , armory_(armory)
   , walletsManager_(walletManager)
   , signContainer_(container)
   , logger_(logger)
   , applicationSettings_(applicationSettings)
   , loadFeeSuggestions_(loadFeeSuggestions)
{
   qRegisterMetaType<std::map<unsigned int, float>>();
}

CreateTransactionDialog::~CreateTransactionDialog() noexcept
{
   if (feeUpdatingThread_.joinable()) {
      feeUpdatingThread_.join();
   }
}

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

   if (signContainer_) {
      connect(signContainer_.get(), &SignContainer::TXSigned, this, &CreateTransactionDialog::onTXSigned);
      connect(signContainer_.get(), &SignContainer::disconnected, this, &CreateTransactionDialog::updateCreateButtonText);
      connect(signContainer_.get(), &SignContainer::authenticated, this, &CreateTransactionDialog::onSignerAuthenticated);
   }
   updateCreateButtonText();
   lineEditAddress()->setFocus();
}

void CreateTransactionDialog::updateCreateButtonText()
{
   if (!signContainer_) {
      pushButtonCreate()->setEnabled(false);
      return;
   }
   if (HaveSignedImportedTransaction()) {
      pushButtonCreate()->setText(tr("Broadcast"));
      if (signContainer_->isOffline()) {
         pushButtonCreate()->setEnabled(false);
      }
      return;
   }
   const auto walletId = UiUtils::getSelectedWalletId(comboBoxWallets());
   if (signContainer_->isOffline() || signContainer_->isWalletOffline(walletId)) {
      pushButtonCreate()->setText(tr("Export"));
   } else {
      selectedWalletChanged(-1);
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
         signContainer_->CancelSignTx(txReq_.serializeState());
      }
   }

   QDialog::reject();
}

void CreateTransactionDialog::closeEvent(QCloseEvent *e)
{
   reject();

   e->ignore();
}

int CreateTransactionDialog::SelectWallet(const std::string& walletId)
{
   auto index = UiUtils::selectWalletInCombobox(comboBoxWallets(), walletId);
   if (index < 0) {
      const auto rootWallet = walletsManager_->getHDRootForLeaf(walletId);
      if (rootWallet) {
         index = UiUtils::selectWalletInCombobox(comboBoxWallets(), rootWallet->walletId());
      }
   }
   return index;
}

void CreateTransactionDialog::populateWalletsList()
{
   int index = UiUtils::fillHDWalletsComboBox(comboBoxWallets(), walletsManager_);
   selectedWalletChanged(index);
}

void CreateTransactionDialog::populateFeeList()
{
   comboBoxFeeSuggestions()->addItem(tr("Loading Fee suggestions"));
   comboBoxFeeSuggestions()->setCurrentIndex(0);
   comboBoxFeeSuggestions()->setEnabled(false);

   connect(this, &CreateTransactionDialog::feeLoadingCompleted
      , this, &CreateTransactionDialog::onFeeSuggestionsLoaded
      , Qt::QueuedConnection);

   loadFees();
}

void CreateTransactionDialog::loadFees()
{
   struct Result {
      std::map<unsigned int, float> values;
      std::set<unsigned int>  levels;
   };
   auto result = std::make_shared<Result>();

   for (const auto &feeLevel : feeLevels) {
      result->levels.insert(feeLevel.first);
   }
   for (const auto &feeLevel : feeLevels) {
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
/*   const auto &cbFees = [this](const std::map<unsigned int, float> &feeMap) {
      emit feeLoadingCompleted(feeMap);
   };
   walletsManager_->getFeeSchedule(cbFees);*/
}

void CreateTransactionDialog::onFeeSuggestionsLoaded(const std::map<unsigned int, float> &feeValues)
{
   comboBoxFeeSuggestions()->clear();

   for (const auto &feeVal : feeValues) {
      QString desc;
      const auto itLevel = feeLevels.find(feeVal.first);
      if (itLevel == feeLevels.end()) {
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
   const auto rootWallet = walletsManager_->getHDWalletById(walletId);
   if (!rootWallet) {
      logger_->error("[{}] wallet with id {} not found", __func__, walletId);
      return;
   }
   if (signContainer_->isWalletOffline(rootWallet->walletId())
      || !rootWallet || signContainer_->isWalletOffline(rootWallet->walletId())) {
      pushButtonCreate()->setText(tr("Export"));
   } else {
      pushButtonCreate()->setText(tr("Broadcast"));
   }
   const auto group = rootWallet->getGroup(rootWallet->getXBTGroupType());
   if ((transactionData_->getGroup() != group) || resetInputs) {
      transactionData_->setGroup(group, armory_->topBlock()
         , resetInputs, cbInputsReset);
   }
}

void CreateTransactionDialog::onTransactionUpdated()
{
   const auto &summary = transactionData_->GetTransactionSummary();

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

   const bs::Address outputAddr(lineEditAddress()->text().trimmed().toStdString());
   const auto maxValue = transactionData_->CalculateMaxAmount(outputAddr);
   if (maxValue > 0) {
      lineEditAmount()->setText(UiUtils::displayAmount(maxValue));
   }
   pushButtonMax()->setEnabled(true);
   lineEditAmount()->setEnabled(true);
}

void CreateTransactionDialog::onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result)
{
   if (!pendingTXSignId_ || (pendingTXSignId_ != id)) {
      return;
   }

   pendingTXSignId_ = 0;
   QString detailedText;

   if (result == bs::error::ErrorCode::TxCanceled) {
      stopBroadcasting();
      return;
   }

   const auto walletId = UiUtils::getSelectedWalletId(comboBoxWallets());
   if (result == bs::error::ErrorCode::NoError && (signContainer_->isOffline() || signContainer_->isWalletOffline(walletId))) {
      // Offline signing
      BSMessageBox(BSMessageBox::info, tr("Offline Transaction")
         , tr("Request was successfully exported")
         , tr("Saved to %1").arg(QString::fromStdString(txReq_.offlineFilePath)), this).exec();
      accept();
      return;
   }
   const auto wallet = walletsManager_->getWalletById(walletId);

   try {
      if (result != bs::error::ErrorCode::NoError) {
         throw std::runtime_error(bs::error::ErrorCodeToString(result).toStdString());
      }

      if (signedTX.isNull()) {
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
      MessageBoxBroadcastError(tr("Invalid signed transaction: %1").arg(QLatin1String(e.what())), this).exec();
      stopBroadcasting();
      return;
   }

   if (result == bs::error::ErrorCode::NoError) {
      if (armory_->broadcastZC(signedTX)) {
         if (!textEditComment()->document()->isEmpty()) {
            const auto &comment = textEditComment()->document()->toPlainText().toStdString();
            transactionData_->getWallet()->setTransactionComment(signedTX, comment);
         }
         accept();
         return;
      }

      detailedText = tr("Failed to communicate to armory to broadcast transaction. Maybe Armory is offline");
   }
   else {
      detailedText = bs::error::ErrorCodeToString(result);
   }

   MessageBoxBroadcastError(detailedText, this).exec();

   stopBroadcasting();
}

void CreateTransactionDialog::startBroadcasting()
{
   broadcasting_ = true;
   pushButtonCreate()->setEnabled(false);
   pushButtonCreate()->setText(tr("Waiting for TX signing..."));
}

void CreateTransactionDialog::stopBroadcasting()
{
   broadcasting_ = false;
   pushButtonCreate()->setEnabled(true);
   updateCreateButtonText();
}

bool CreateTransactionDialog::BroadcastImportedTx()
{
   if (importedSignedTX_.isNull()) {
      return false;
   }
   startBroadcasting();
   if (armory_->broadcastZC(importedSignedTX_)) {
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

bool CreateTransactionDialog::CreateTransaction()
{
   const auto changeAddress = getChangeAddress();
   QString text;
   QString detailedText;

   if (!signContainer_) {
      BSMessageBox(BSMessageBox::critical, tr("Error")
         , tr("Signer is invalid - unable to send transaction"), this).exec();
      return false;
   }

   std::string offlineFilePath;
   const auto hdWallet = walletsManager_->getHDWalletById(UiUtils::getSelectedWalletId(comboBoxWallets()));

   if (hdWallet->isOffline()) {
      QString signerOfflineDir = applicationSettings_->get<QString>(ApplicationSettings::signerOfflineDir);

      const qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
      const std::string fileName = fmt::format("{}_{}.bin", hdWallet->walletId(), timestamp);

      QString defaultFilePath = QDir(signerOfflineDir).filePath(QString::fromStdString(fileName));
      offlineFilePath = QFileDialog::getSaveFileName(this, tr("Save Offline TX as..."), defaultFilePath).toStdString();

      if (offlineFilePath.empty()) {
         return true;
      }

      QString newSignerOfflineDir = QFileInfo(QString::fromStdString(offlineFilePath)).absoluteDir().path();
      if (signerOfflineDir != newSignerOfflineDir) {
         applicationSettings_->set(ApplicationSettings::signerOfflineDir, newSignerOfflineDir);
      }
   }

   startBroadcasting();
   try {
      txReq_ = transactionData_->createTXRequest(checkBoxRBF()->checkState() == Qt::Checked
         , changeAddress, originalFee_);
      txReq_.comment = textEditComment()->document()->toPlainText().toStdString();
      txReq_.offlineFilePath = offlineFilePath;

      // We shouldn't hit this case since the request checks the incremental
      // relay fee requirement for RBF. But, in case we
      if (txReq_.fee <= originalFee_) {
         BSMessageBox(BSMessageBox::info, tr("Error"), tr("Fee is too low"),
            tr("Due to RBF requirements, the current fee (%1) will be " \
               "increased 1 satoshi above the original transaction fee (%2)")
            .arg(UiUtils::displayAmount(txReq_.fee))
            .arg(UiUtils::displayAmount(originalFee_))).exec();
         txReq_.fee = originalFee_ + 1;
      }

      const float newFeePerByte = transactionData_->feePerByte();
      if ((originalFeePerByte_ - newFeePerByte) > 0.005) {  // allow some rounding
         BSMessageBox(BSMessageBox::info, tr("Error"), tr("Fee per byte is too low"),
            tr("Due to RBF requirements, the current fee per byte (%1) will " \
               "be increased to the original transaction fee rate (%2)")
            .arg(newFeePerByte)
            .arg(originalFeePerByte_)).exec();
         txReq_.fee = std::ceil(txReq_.fee * (originalFeePerByte_ / newFeePerByte));
      }

      pendingTXSignId_ = signContainer_->signTXRequest(txReq_, SignContainer::TXSignMode::Full, true);
      if (!pendingTXSignId_) {
         throw std::logic_error("Signer failed to send request");
      }
      return true;
   }
   catch (const std::runtime_error &e) {
      text = tr("Failed to broadcast transaction");
      detailedText = QString::fromStdString(e.what());
      qDebug() << QLatin1String("[CreateTransactionDialog::CreateTransaction] exception: ") << QString::fromStdString(e.what());
   }
   catch (const std::logic_error &e) {
      text = tr("Failed to create transaction");
      detailedText = QString::fromStdString(e.what());
      qDebug() << QLatin1String("[CreateTransactionDialog::CreateTransaction] exception: ") << QString::fromStdString(e.what());
   }
   catch (const std::exception &e) {
      text = tr("Failed to create transaction");
      detailedText = QString::fromStdString(e.what());
      qDebug() << QLatin1String("[CreateTransactionDialog::CreateTransaction] exception: ") << QString::fromStdString(e.what());
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
      , tr("TX files (*.bin)"));
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

   auto transactions = ParseOfflineTXFile(data);
   if (transactions.size() != 1) {
      showError(title, tr("Invalid file %1 format").arg(reqFile));
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
