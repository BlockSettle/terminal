/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CreateTransactionDialogSimple.h"

#include "ui_CreateTransactionDialogSimple.h"

#include "Address.h"
#include "ArmoryConnection.h"
#include "BSMessageBox.h"
#include "CreateTransactionDialogAdvanced.h"
#include "SignContainer.h"
#include "TransactionData.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "XbtAmountValidator.h"

CreateTransactionDialogSimple::CreateTransactionDialogSimple(uint32_t topBlock
   , const std::shared_ptr<spdlog::logger>& logger, QWidget* parent)
   : CreateTransactionDialog(true, topBlock, logger, parent)
   , ui_(new Ui::CreateTransactionDialogSimple)
{
   ui_->setupUi(this);
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
   bs::Address address;
   try {
      address = bs::Address::fromAddressString(addressString.trimmed().toStdString());
   } catch (...) {
   }
   bool addrStateOk = address.isValid() && (address.format() != bs::Address::Format::Hex);
   // Always update address (to make transactionData_ invalid if needed)
   transactionData_->UpdateRecipientAddress(recipientId_, address);
   UiUtils::setWrongState(ui_->lineEditAddress, !addrStateOk);
   ui_->pushButtonMax->setEnabled(addrStateOk);
}

void CreateTransactionDialogSimple::onXBTAmountChanged(const QString &text)
{
   const bs::XBTAmount value{ UiUtils::parseAmountBtc(text) };
   transactionData_->UpdateRecipientAmount(recipientId_, value);
}

void CreateTransactionDialogSimple::onMaxPressed()
{
   transactionData_->UpdateRecipientAmount(recipientId_, {}, false);
   CreateTransactionDialog::onMaxPressed();
   const bs::XBTAmount value{ UiUtils::parseAmountBtc(ui_->lineEditAmount->text()) };
   transactionData_->UpdateRecipientAmount(recipientId_, value, true);
}

void CreateTransactionDialogSimple::onTransactionUpdated()
{
   if (!advancedDialogRequested_) {
      if (transactionData_->GetRecipientAmount(recipientId_).isZero()) {
         UiUtils::setWrongState(ui_->lineEditAmount, false);
      }
      else {
         UiUtils::setWrongState(ui_->lineEditAmount, !transactionData_->IsTransactionValid());
      }
      CreateTransactionDialog::onTransactionUpdated();
   }
}

void CreateTransactionDialogSimple::showAdvanced()
{
   advancedDialogRequested_ = true;
   accept();
}

void CreateTransactionDialogSimple::getChangeAddress(AddressCb cb) const
{
   if (transactionData_->GetTransactionSummary().hasChange) {
      transactionData_->getWallet()->getNewChangeAddress([cb = std::move(cb)](const bs::Address &addr) {
         cb(addr);
      });
      return;
   }
   cb({});
}

void CreateTransactionDialogSimple::createTransaction()
{
   if (!importedSignedTX_.empty()) {
      if (BroadcastImportedTx()) {
         accept();
      }
      else {
         initUI();
      }
      return;
   }

   CreateTransaction([this, handle = validityFlag_.handle()](bool result, const std::string &errorMsg, const std::string& unsignedTx, uint64_t virtSize) {
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

bool CreateTransactionDialogSimple::switchModeRequested() const
{
   return advancedDialogRequested_;
}

std::shared_ptr<CreateTransactionDialog> CreateTransactionDialogSimple::SwitchMode()
{
   if (!paymentInfo_.address.isEmpty()) {
      return CreateTransactionDialogAdvanced::CreateForPaymentRequest(topBlock_
         , logger_, paymentInfo_, parentWidget());
   }

   auto advancedDialog = std::make_shared<CreateTransactionDialogAdvanced>(topBlock_
      , true, logger_, transactionData_, std::move(utxoRes_), parentWidget());

   if (!offlineTransactions_.empty()) {
      advancedDialog->SetImportedTransactions(offlineTransactions_);
   } else {
      // select wallet
      advancedDialog->SelectWallet(UiUtils::getSelectedWalletId(ui_->comboBoxWallets),
         UiUtils::getSelectedWalletType(ui_->comboBoxWallets));

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

void CreateTransactionDialogSimple::preSetAddress(const QString& address)
{
   ui_->lineEditAddress->setText(address);
   onAddressTextChanged(address);
}

void CreateTransactionDialogSimple::preSetValue(const double value)
{
   ui_->lineEditAmount->setText(UiUtils::displayAmount(value));
}

void CreateTransactionDialogSimple::preSetValue(const bs::XBTAmount& value)
{
   ui_->lineEditAmount->setText(UiUtils::displayAmount(value));
}

std::shared_ptr<CreateTransactionDialog> CreateTransactionDialogSimple::CreateForPaymentRequest(uint32_t topBlock
   , const std::shared_ptr<spdlog::logger>& logger
   , const Bip21::PaymentRequestInfo& paymentInfo
   , QWidget* parent)
{
   if (!canUseSimpleMode(paymentInfo)) {
      return CreateTransactionDialogAdvanced::CreateForPaymentRequest(topBlock
         , logger, paymentInfo, parent);
   }

   auto dlg = std::make_shared<CreateTransactionDialogSimple>(topBlock, logger
      , parent);

   // set address and lock
   dlg->preSetAddress(paymentInfo.address);
   dlg->ui_->lineEditAddress->setEnabled(false);

   // set amount and lock
   if (paymentInfo.amount.GetValue() != 0) {
      dlg->preSetValue(paymentInfo.amount);
      dlg->ui_->lineEditAmount->setEnabled(false);
      dlg->ui_->pushButtonMax->setEnabled(false);
   }

   // set message or label to comment
   if (!paymentInfo.message.isEmpty()) {
      dlg->ui_->textEditComment->setText(paymentInfo.message);
   } else if (!paymentInfo.label.isEmpty()) {
      dlg->ui_->textEditComment->setText(paymentInfo.label);
   }

   dlg->ui_->checkBoxRBF->setChecked(false);
   dlg->ui_->checkBoxRBF->setEnabled(false);
   dlg->ui_->checkBoxRBF->setToolTip(tr("RBF disabled for payment request"));

   dlg->paymentInfo_ = paymentInfo;

   return dlg;
}
