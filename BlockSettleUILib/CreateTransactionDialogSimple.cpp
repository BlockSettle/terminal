#include "CreateTransactionDialogSimple.h"

#include "ui_CreateTransactionDialogSimple.h"

#include "Address.h"
#include "ArmoryConnection.h"
#include "CreateTransactionDialogAdvanced.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "XbtAmountValidator.h"

#include <QFileDialog>

CreateTransactionDialogSimple::CreateTransactionDialogSimple(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::sync::WalletsManager>& walletManager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<spdlog::logger>& logger
   , const std::shared_ptr<ApplicationSettings> &applicationSettings
   , QWidget* parent)
   : CreateTransactionDialog(armory, walletManager, container, true, logger, applicationSettings, parent)
   , ui_(new Ui::CreateTransactionDialogSimple)
{
   ui_->setupUi(this);
   initUI();
}

CreateTransactionDialogSimple::~CreateTransactionDialogSimple() = default;

void CreateTransactionDialogSimple::initUI()
{
   CreateTransactionDialog::init();

   recipientId_ = transactionData_->RegisterNewRecipient();

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
   return ui_->labelInputAmount;
}

QLabel *CreateTransactionDialogSimple::labelTxInputs() const
{
   return ui_->labelTXInputs;
}

QLabel *CreateTransactionDialogSimple::labelEstimatedFee() const
{
   return ui_->labelFee;
}

QLabel *CreateTransactionDialogSimple::labelTotalAmount() const
{
   return ui_->labelTransationAmount;
}

QLabel *CreateTransactionDialogSimple::labelTxSize() const
{
   return ui_->labelTxSize;
}

QPushButton *CreateTransactionDialogSimple::pushButtonCreate() const
{
   return ui_->pushButtonCreate;
}

QPushButton *CreateTransactionDialogSimple::pushButtonCancel() const
{
   return ui_->pushButtonCancel;
}

QLabel* CreateTransactionDialogSimple::feePerByteLabel() const
{
   return ui_->labelFeePerByte;
}

QLabel* CreateTransactionDialogSimple::changeLabel() const
{
   return ui_->labelReturnAmount;
}

void CreateTransactionDialogSimple::onAddressTextChanged(const QString &addressString)
{
   bool addrStateOk = true;
   try {
      bs::Address address{ addressString.trimmed().toStdString() };
      addrStateOk = address.isValid() && (address.format() != bs::Address::Format::Hex);
      if (addrStateOk) {
         transactionData_->UpdateRecipientAddress(recipientId_, address);
      }
   } catch (...) {
      addrStateOk = false;
   }
   UiUtils::setWrongState(ui_->lineEditAddress, !addrStateOk);
   ui_->pushButtonMax->setEnabled(addrStateOk);
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

bs::Address CreateTransactionDialogSimple::getChangeAddress() const
{
   auto promAddr = std::make_shared<std::promise<bs::Address>>();
   auto futAddr = promAddr->get_future();
   const auto &cbAddr = [promAddr](const bs::Address &addr) {
      promAddr->set_value(addr);
   };
   if (transactionData_->GetTransactionSummary().hasChange) {
      transactionData_->getWallet()->getNewChangeAddress(cbAddr);
      return futAddr.get();
   }
   return {};
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
   auto advancedDialog = std::make_shared<CreateTransactionDialogAdvanced>(armory_, walletsManager_
      , signContainer_, true, logger_, applicationSettings_, transactionData_, parentWidget());

   if (!offlineTransactions_.empty()) {
      advancedDialog->SetImportedTransactions(offlineTransactions_);
   } else {
      // select wallet
      advancedDialog->SelectWallet(UiUtils::getSelectedWalletId(ui_->comboBoxWallets));

      // set inputs and amounts
      auto address = ui_->lineEditAddress->text().trimmed();
      if (!address.isEmpty()) {
         advancedDialog->preSetAddress(address);
      }

      auto valueText = ui_->lineEditAmount->text();
      if (!valueText.isEmpty()) {
         double value = UiUtils::parseAmountBtc(valueText);
         advancedDialog->preSetValue(value);
      }
   }

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

QLabel* CreateTransactionDialogSimple::labelTXAmount() const
{
   return ui_->labelTransactionAmount;
}

QLabel* CreateTransactionDialogSimple::labelTxOutputs() const
{
   return ui_->labelTXOutputs;
}
