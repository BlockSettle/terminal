/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CREATE_TRANSACTION_DIALOG_ADVANCED_H__
#define __CREATE_TRANSACTION_DIALOG_ADVANCED_H__

#include "CreateTransactionDialog.h"
#include "CoreWallet.h"

namespace Ui {
    class CreateTransactionDialogAdvanced;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Group;
      }
      class Wallet;
      class WalletsManager;
   }
}


class CreateTransactionDialogAdvanced : public CreateTransactionDialog
{
Q_OBJECT

public:
   static std::shared_ptr<CreateTransactionDialogAdvanced>  CreateForRBF(
        const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<SignContainer>&
      , const std::shared_ptr<spdlog::logger>&
      , const std::shared_ptr<ApplicationSettings> &
      , const Tx &
      , QWidget* parent = nullptr);

   static std::shared_ptr<CreateTransactionDialogAdvanced>  CreateForCPFP(
        const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager>&
      , const std::shared_ptr<SignContainer>&
      , const std::shared_ptr<bs::sync::Wallet>&
      , const std::shared_ptr<spdlog::logger>&
      , const std::shared_ptr<ApplicationSettings> &
      , const Tx &
      , QWidget* parent = nullptr);

public:
   CreateTransactionDialogAdvanced(const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager> &, const std::shared_ptr<SignContainer> &
      , bool loadFeeSuggestions, const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ApplicationSettings> &applicationSettings
      , const std::shared_ptr<TransactionData> &
      , QWidget* parent = nullptr);
   ~CreateTransactionDialogAdvanced() override;

   void preSetAddress(const QString& address);
   void preSetValue(const double value);

   void SetImportedTransactions(const std::vector<bs::core::wallet::TXSignRequest>& transactions);

protected:
   bool eventFilter(QObject *watched, QEvent *) override;

   QComboBox *comboBoxWallets() const override;
   QComboBox *comboBoxFeeSuggestions() const override;
   QLineEdit *lineEditAddress() const override;
   QLineEdit *lineEditAmount() const override;
   QPushButton *pushButtonMax() const override;
   QTextEdit *textEditComment() const override;
   QCheckBox *checkBoxRBF() const override;
   QLabel *labelBalance() const override;
   QLabel *labelAmount() const override;
   QLabel *labelTxInputs() const override;
   QLabel *labelEstimatedFee() const override;
   QLabel *labelTotalAmount() const override;
   QLabel *labelTxSize() const override;
   QPushButton *pushButtonCreate() const override;
   QPushButton *pushButtonCancel() const override;

   QLabel* labelTXAmount() const override;
   QLabel* labelTxOutputs() const override;

   virtual QLabel *feePerByteLabel() const override;
   virtual QLabel *changeLabel() const override;

   void onTransactionUpdated() override;
   void getChangeAddress(AddressCb cb) const override;

   bool HaveSignedImportedTransaction() const override;

protected slots:
   void onAddressTextChanged(const QString& addressString);
   void onFeeSuggestionsLoaded(const std::map<unsigned int, float> &) override;
   void onXBTAmountChanged(const QString& text);

   void onSelectInputs();
   void onAddOutput();
   void onCreatePressed();
   void onImportPressed();

   void feeSelectionChanged(int currentIndex) override;

   void onNewAddressSelectedForChange();
   void onExistingAddressSelectedForChange();

   void showContextMenu(const QPoint& point);
   void onRemoveOutput();

private slots:
   void updateManualFeeControls();
   void setTxFees();
   void onOutputsClicked(const QModelIndex &index);

private:
   void clear() override;
   void initUI();

   void setRBFinputs(const Tx &);
   void setCPFPinputs(const Tx &, const std::shared_ptr<bs::sync::Wallet> &);

   bool isCurrentAmountValid() const;
   void validateAddOutputButton();
   Q_INVOKABLE void validateCreateButton();

   unsigned int AddRecipient(const bs::Address &, double amount, bool isMax = false);
   void AddRecipients(const std::vector<std::tuple<bs::Address, double, bool>> &);
   void UpdateRecipientAmount(unsigned int recipId, double amount, bool isMax = false);
   bool FixRecipientsAmount();
   void onOutputRemoved();

   void AddManualFeeEntries(float feePerByte, float totalFee);
   void setAdvisedFees(float totalFee, float feePerByte);
   void SetMinimumFee(float totalFee, float feePerByte = 0);

   void SetFixedWallet(const std::string& walletId, const std::function<void()> &cbInputsReset = nullptr);
   void setFixedGroupInputs(const std::shared_ptr<bs::sync::hd::Group> &, const std::vector<UTXO> &);
   void SetInputs(const std::vector<UTXO> &);
   void disableOutputsEditing();
   void disableInputSelection();
   void enableFeeChanging(bool flag = true);
   void SetFixedChangeAddress(const QString& changeAddress);
   void SetPredefinedFee(const int64_t& manualFee);
   void setUnchangeableTx();

   void RemoveOutputByRow(int row);

   void showExistingChangeAddress(bool show);

   void disableChangeAddressSelecting();

   void fixFeePerByte();

private:
   std::unique_ptr<Ui::CreateTransactionDialogAdvanced> ui_;

   bs::Address currentAddress_;
   double      currentValue_ = 0;
   bool     isRBF_ = false;
   bool     allowAutoSelInputs_ = true;

   UsedInputsModel         *  usedInputsModel_ = nullptr;
   TransactionOutputsModel *  outputsModel_ = nullptr;

   bool        removeOutputEnabled_ = true;
   QMenu       contextMenu_;
   QAction *   removeOutputAction_ = nullptr;

   bool        changeAddressFixed_ = false;
   bs::Address selectedChangeAddress_;

   bool        feeChangeDisabled_ = false;

   bool        showUnknownWalletWarning_ = false;
};

#endif // __CREATE_TRANSACTION_DIALOG_ADVANCED_H__
