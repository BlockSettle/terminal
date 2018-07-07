#include "CreateTransactionDialog.h"
#include <stdexcept>
#include <thread>
#include <QDebug>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QIntValidator>
#include <QFile>
#include <QFileDialog>
#include "Address.h"
#include "CoinControlDialog.h"
#include "MessageBoxCritical.h"
#include "MessageBoxInfo.h"
#include "MessageBoxQuestion.h"
#include "OfflineSigner.h"
#include "PyBlockDataManager.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "TransactionOutputsModel.h"
#include "UiUtils.h"
#include "UsedInputsModel.h"
#include "WalletsManager.h"
#include "XbtAmountValidator.h"


const std::map<unsigned int, QString> feeLevels = { {2, QObject::tr("20 minutes") },
   { 4, QObject::tr("40 minutes") }, { 6, QObject::tr("1 hour") }, { 12, QObject::tr("2 hours") },
   { 24, QObject::tr("4 hours") }, { 48, QObject::tr("8 hours") }, { 144, QObject::tr("24 hours") },
   { 504, QObject::tr("3 days") }, { 1008, QObject::tr("7 days") }
};

CreateTransactionDialog::CreateTransactionDialog(const std::shared_ptr<WalletsManager>& walletManager
   , const std::shared_ptr<SignContainer> &container, bool loadFeeSuggestions, QWidget* parent)
   : QDialog(parent)
   , walletsManager_(walletManager)
   , signingContainer_(container)
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
   transactionData_ = std::make_shared<TransactionData>([this]() { onTransactionUpdated(); });

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

   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::TXSigned, this, &CreateTransactionDialog::onTXSigned);
      updateCreateButtonText();
   }
   lineEditAddress()->setFocus();
}

void CreateTransactionDialog::updateCreateButtonText()
{
   if (signingContainer_) {
      if (signingContainer_->opMode() == SignContainer::OpMode::Offline) {
         if (HaveSignedImportedTransaction()) {
            pushButtonCreate()->setText(tr("Broadcast"));
         } else {
            pushButtonCreate()->setText(tr("Export"));
         }
      } else {
         if (signingContainer_->isOffline()) {
            pushButtonCreate()->setText(tr("Unable to sign"));
         } else {
            pushButtonCreate()->setText(tr("Broadcast"));
         }
      }
   }
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
      MessageBoxQuestion confirmExit(tr("Abort transaction broadcasting")
         , tr("You're about to abort transaction sending")
         , tr("Are you sure you wish to abort the signing and sending process?"), this);
      confirmExit.setExclamationIcon();
      if (confirmExit.exec() != QDialog::Accepted) {
         return;
      }
   }
   QDialog::reject();
}

void CreateTransactionDialog::SelectWallet(const std::string& walletId)
{
   UiUtils::selectWalletInCombobox(comboBoxWallets(), walletId);
}

void CreateTransactionDialog::populateWalletsList()
{
   auto selectedWalletIndex = UiUtils::fillWalletsComboBox(comboBoxWallets(), walletsManager_, signingContainer_);
   selectedWalletChanged(selectedWalletIndex);
}

void CreateTransactionDialog::populateFeeList()
{
   comboBoxFeeSuggestions()->addItem(tr("Loading Fee suggestions"));
   comboBoxFeeSuggestions()->setCurrentIndex(0);
   comboBoxFeeSuggestions()->setEnabled(false);

   connect(this, &CreateTransactionDialog::feeLoadingColmpleted
      , this, &CreateTransactionDialog::onFeeSuggestionsLoaded
      , Qt::QueuedConnection);

   feeUpdatingThread_ = std::thread(&CreateTransactionDialog::feeUpdatingThreadFunction, this);
}

void CreateTransactionDialog::feeUpdatingThreadFunction()
{
   std::map<unsigned int, float> feeValues;
   for (const auto &feeLevel : feeLevels) {
      feeValues[feeLevel.first] = walletsManager_->estimatedFeePerByte(feeLevel.first);
   }
   emit feeLoadingColmpleted(feeValues);
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
   transactionData_->SetFeePerByte(comboBoxFeeSuggestions()->itemData(currentIndex).toFloat());
}

void CreateTransactionDialog::selectedWalletChanged(int)
{
   auto currentWallet = walletsManager_->GetWalletById(UiUtils::getSelectedWalletId(comboBoxWallets()));
   transactionData_->SetWallet(currentWallet);
}

void CreateTransactionDialog::onTransactionUpdated()
{
   const auto &summary = transactionData_->GetTransactionSummary();

   labelBalance()->setText(UiUtils::displayAmount(summary.availableBalance));
   labelAmount()->setText(UiUtils::displayAmount(summary.selectedBalance));
   labelTxInputs()->setText(summary.isAutoSelected ? tr("Auto (%1)").arg(QString::number(summary.usedTransactions))
      : QString::number(summary.usedTransactions));
   labelEstimatedFee()->setText(UiUtils::displayAmount(summary.totalFee));
   labelTotalAmount()->setText(UiUtils::displayAmount(UiUtils::amountToBtc(summary.balanceToSpent) + UiUtils::amountToBtc(summary.totalFee)));
   if (labelTxSize()) {
      labelTxSize()->setText(QString::number(summary.transactionSize) + tr(" bytes"));
   }

   if (feePerByteLabel() != nullptr) {
      feePerByteLabel()->setText(tr("%1 s/b").arg(summary.feePerByte));
   }

   if (changeLabel() != nullptr) {
      if (summary.hasChange) {
         changeLabel()->setText(UiUtils::displayAmount(summary.selectedBalance - UiUtils::amountToBtc(summary.balanceToSpent) - UiUtils::amountToBtc(summary.totalFee)));
      } else {
         changeLabel()->setText(UiUtils::displayAmount(0.0));
      }
   }
}

void CreateTransactionDialog::onMaxPressed()
{
   auto maxValue = transactionData_->CalculateMaxAmount(lineEditAddress()->text().toStdString());
   lineEditAmount()->setText(UiUtils::displayAmount(maxValue));
}

void CreateTransactionDialog::onTXSigned(unsigned int id, BinaryData signedTX, std::string error)
{
   if (!pendingTXSignId_ || (pendingTXSignId_ != id)) {
      return;
   }

   pendingTXSignId_ = 0;
   QString detailedText;

   if (error.empty()) {
      if (signingContainer_->isOffline()) {   // Offline signing
         MessageBoxInfo(tr("Offline Transacation")
            , tr("Request exported to:\n%1").arg(QString::fromStdString(signedTX.toBinStr()))
            , this).exec();
         accept();
         return;
      }

      if (PyBlockDataManager::instance()->broadcastZC(signedTX)) {
         if (!textEditComment()->document()->isEmpty()) {
            const auto &comment = textEditComment()->document()->toPlainText().toStdString();
            transactionData_->GetWallet()->SetTransactionComment(signedTX, comment);
         }
         accept();
         return;
      }

      detailedText = tr("Failed to communicate to armory to broadcast transaction. Maybe Armory is offline");
   }
   else {
      detailedText = QString::fromStdString(error);
   }

   MessageBoxBroadcastError(detailedText, this).exec();
   stopBroadcasting();
}

void CreateTransactionDialog::startBroadcasting()
{
   broadcasting_ = true;
   pushButtonCancel()->setEnabled(false);
   pushButtonCreate()->setEnabled(false);
   pushButtonCreate()->setText(tr("Waiting for TX signing..."));
}

void CreateTransactionDialog::stopBroadcasting()
{
   broadcasting_ = false;
   pushButtonCancel()->setEnabled(true);
   pushButtonCreate()->setEnabled(true);
   updateCreateButtonText();
}

bool CreateTransactionDialog::BroadcastImportedTx()
{
   if (importedSignedTX_.isNull()) {
      return false;
   }
   startBroadcasting();
   if (PyBlockDataManager::instance()->broadcastZC(importedSignedTX_)) {
      if (!textEditComment()->document()->isEmpty()) {
         const auto &comment = textEditComment()->document()->toPlainText().toStdString();
         transactionData_->GetWallet()->SetTransactionComment(importedSignedTX_, comment);
      }
      return true;
   }
   importedSignedTX_.clear();
   stopBroadcasting();
   MessageBoxCritical(tr("Transaction broadcast"), tr("Failed to broadcast imported transaction"), this).exec();
   return false;
}

bool CreateTransactionDialog::CreateTransaction()
{
   const auto changeAddress = getChangeAddress();
   QString text;
   QString detailedText;

   startBroadcasting();
   try {
      signingContainer_->SyncAddresses(transactionData_->createAddresses());

      auto txReq = transactionData_->CreateTXRequest(checkBoxRBF()->checkState() == Qt::Checked
         , changeAddress);
      txReq.comment = textEditComment()->document()->toPlainText().toStdString();

      if (txReq.fee <= originalFee_) {
         MessageBoxCritical(tr("Fee is low"),
            tr("Your current fee (%1) should exceed the fee from the original transaction (%2)")
            .arg(UiUtils::displayAmount(txReq.fee)).arg(UiUtils::displayAmount(originalFee_))).exec();
         stopBroadcasting();
         return true;
      }

      pendingTXSignId_ = signingContainer_->SignTXRequest(txReq, false,
         SignContainer::TXSignMode::Full, {}, true);
      if (!pendingTXSignId_) {
         throw std::logic_error("Signer failed to send request");
      }
      return true;
   }
   catch (const std::runtime_error &e) {
      text = tr("Failed to broadcast transaction");
      detailedText = QString::fromStdString(e.what());
      qDebug() << QLatin1String("[CreateTransactionDialogAdvanced::onCreatePressed] exception: ") << QString::fromStdString(e.what());
   }
   catch (const std::logic_error &e) {
      text = tr("Failed to create transaction");
      detailedText = QString::fromStdString(e.what());
      qDebug() << QLatin1String("[CreateTransactionDialogAdvanced::onCreatePressed] exception: ") << QString::fromStdString(e.what());
   }
   catch (const std::exception &e) {
      text = tr("Failed to create transaction");
      detailedText = QString::fromStdString(e.what());
      qDebug() << QLatin1String("[CreateTransactionDialogAdvanced::onCreatePressed] exception: ") << QString::fromStdString(e.what());
   }

   stopBroadcasting();
   showError(text, detailedText);
   return false;
}

std::vector<bs::wallet::TXSignRequest> CreateTransactionDialog::ImportTransactions()
{
   const auto reqFile = QFileDialog::getOpenFileName(this, tr("Select Transaction file"), offlineDir_
      , tr("TX files (*.bin)"));
   if (reqFile.isEmpty()) {
      return {};
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

   const auto &transactions = ParseOfflineTXFile(data);
   if (transactions.size() != 1) {
      showError(title, tr("Invalid file %1 format").arg(reqFile));
      return {};
   }

   clear();
   return transactions;
}

void CreateTransactionDialog::showError(const QString &text, const QString &detailedText)
{
   MessageBoxCritical errorMessage(text, detailedText, this);
   errorMessage.exec();
}
