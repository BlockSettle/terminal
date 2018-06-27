#include "CreateTransactionDialogSimple.h"

#include "ui_CreateTransactionDialogSimple.h"

#include "Address.h"
#include "CreateTransactionDialogAdvanced.h"
#include "MessageBoxCritical.h"
#include "MessageBoxInfo.h"
#include "OfflineSigner.h"
#include "PyBlockDataManager.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "WalletsManager.h"
#include "XbtAmountValidator.h"

#include <QFileDialog>
#include <QDebug>

constexpr int highPriorityBlocksNumber = 0;
constexpr int normalPriorityBlocksNumber = 3;
constexpr int lowPriorityBlocksNumber = 6;

CreateTransactionDialogSimple::CreateTransactionDialogSimple(const std::shared_ptr<WalletsManager>& walletManager
   , const std::shared_ptr<SignContainer> &container, QWidget* parent)
 : CreateTransactionDialog(walletManager, container, true, parent)
 , ui_(new Ui::CreateTransactionDialogSimple)
{
   ui_->setupUi(this);
   initUI();
}

void CreateTransactionDialogSimple::initUI()
{
   CreateTransactionDialog::init();

   recipientId_ = transactionData_->RegisterNewRecipient();

   connect(ui_->comboBoxWallets, SIGNAL(currentIndexChanged(int)), this, SLOT(selectedWalletChanged(int)));

   connect(ui_->lineEditAddress, &QLineEdit::textEdited, this, &CreateTransactionDialogSimple::onAddressTextChanged);
   connect(ui_->lineEditAmount, &QLineEdit::textChanged, this, &CreateTransactionDialogSimple::onXBTAmountChanged);

   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, &CreateTransactionDialogSimple::createTransaction);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &QDialog::reject);
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, &CreateTransactionDialogSimple::onImportPressed);

   connect(ui_->pushButtonShowAdvanced, &QPushButton::clicked, this, &CreateTransactionDialogSimple::showAdvanced);
}

QComboBox *CreateTransactionDialogSimple::comboBoxWallets() const
{
   return ui_->comboBoxWallets;
}

QComboBox *CreateTransactionDialogSimple::comboBoxFeeSuggestions() const
{
   return ui_->comboBoxFeeSuggestions;
}

QLineEdit *CreateTransactionDialogSimple::lineEditAddress() const
{
   return ui_->lineEditAddress;
}

QLineEdit *CreateTransactionDialogSimple::lineEditAmount() const
{
   return ui_->lineEditAmount;
}

QPushButton *CreateTransactionDialogSimple::pushButtonMax() const
{
   return ui_->pushButtonMax;
}

QTextEdit *CreateTransactionDialogSimple::textEditComment() const
{
   return ui_->textEditComment;
}

QCheckBox *CreateTransactionDialogSimple::checkBoxRBF() const
{
   return ui_->checkBoxRBF;
}

QLabel *CreateTransactionDialogSimple::labelBalance() const
{
   return ui_->labelBalance;
}

QLabel *CreateTransactionDialogSimple::labelAmount() const
{
   return ui_->labelDetailsAmount;
}

QLabel *CreateTransactionDialogSimple::labelTxInputs() const
{
   return ui_->labelDetailsTxInputs;
}

QLabel *CreateTransactionDialogSimple::labelEstimatedFee() const
{
   return ui_->labelDetailsEstimatedFee;
}

QLabel *CreateTransactionDialogSimple::labelTotalAmount() const
{
   return ui_->labelDetailsTotalAmount;
}

QPushButton *CreateTransactionDialogSimple::pushButtonCreate() const
{
   return ui_->pushButtonCreate;
}

QPushButton *CreateTransactionDialogSimple::pushButtonCancel() const
{
   return ui_->pushButtonCancel;
}

void CreateTransactionDialogSimple::onAddressTextChanged(const QString &addressString)
{
   try {
      bs::Address address{addressString.trimmed()};
      transactionData_->UpdateRecipientAddress(recipientId_, address);
      if (address.isValid()) {
         ui_->pushButtonMax->setEnabled(true);
         UiUtils::setWrongState(ui_->lineEditAddress, false);
         return;
      } else {
         UiUtils::setWrongState(ui_->lineEditAddress, true);
      }
   } catch(...) {
      UiUtils::setWrongState(ui_->lineEditAddress, true);
   }

   ui_->pushButtonMax->setEnabled(false);
   transactionData_->ResetRecipientAddress(recipientId_);
}

void CreateTransactionDialogSimple::onXBTAmountChanged(const QString &text)
{
   double value = UiUtils::parseAmountBtc(text);
   transactionData_->UpdateRecipientAmount(recipientId_, value);
}

void CreateTransactionDialogSimple::onMaxPressed()
{
   CreateTransactionDialog::onMaxPressed();
   transactionData_->UpdateRecipientAmount(recipientId_, UiUtils::parseAmountBtc(ui_->lineEditAmount->text()), true);
}

void CreateTransactionDialogSimple::showAdvanced()
{
   advancedDialogRequested_ = true;
   accept();
}

void CreateTransactionDialogSimple::onTransactionUpdated()
{
   CreateTransactionDialog::onTransactionUpdated();

   ui_->pushButtonCreate->setEnabled(transactionData_->IsTransactionValid());
}

bs::Address CreateTransactionDialogSimple::getChangeAddress() const
{
   bs::Address result;
   if (transactionData_->GetTransactionSummary().hasChange) {
      result = transactionData_->GetWallet()->GetNewChangeAddress();
   }
   return result;
}

void CreateTransactionDialogSimple::createTransaction()
{
   if (!importedSignedTX_.isNull()) {
      if (!importedSignedTX_.isNull()) {
         if (BroadcastImportedTx()) {
            accept();
         }
         else {
            initUI();
         }
         return;
      }
   }

   if (!CreateTransaction()) {
      reject();
   }
}

bool CreateTransactionDialogSimple::userRequestedAdvancedDialog() const
{
   return advancedDialogRequested_;
}

std::shared_ptr<CreateTransactionDialogAdvanced> CreateTransactionDialogSimple::CreateAdvancedDialog()
{
   auto advancedDialog = std::make_shared<CreateTransactionDialogAdvanced>(walletsManager_
      , signingContainer_, true, parentWidget());

   if (!offlineTransactions_.empty()) {
      advancedDialog->SetImportedTransactions(offlineTransactions_);
   } else {
      // select wallet
      advancedDialog->SelectWallet(UiUtils::getSelectedWalletId(ui_->comboBoxWallets));

      // set inputs and amounts
      auto address = ui_->lineEditAddress->text();
      if (!address.isEmpty()) {
         advancedDialog->preSetAddress(address);
      }

      auto valueText = ui_->lineEditAmount->text();
      if (!valueText.isEmpty()) {
         double value = UiUtils::parseAmountBtc(valueText);
         advancedDialog->preSetValue(value);
      }
   }

   advancedDialog->setOfflineDir(offlineDir_);

   return advancedDialog;
}

void CreateTransactionDialogSimple::onImportPressed()
{
   offlineTransactions_ = ImportTransactions();
   if (offlineTransactions_.empty()) {
      return;
   }

   showAdvanced();
}
