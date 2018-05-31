#ifndef __CREATE_TRANSACTION_DIALOG_ADVANCED_H__
#define __CREATE_TRANSACTION_DIALOG_ADVANCED_H__

#include "CreateTransactionDialog.h"

namespace Ui {
    class CreateTransactionDialogAdvanced;
}


class CreateTransactionDialogAdvanced : public CreateTransactionDialog
{
Q_OBJECT

public:
   static std::shared_ptr<CreateTransactionDialogAdvanced>  CreateForRBF(
        const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer>&
      , const Tx &
      , const std::shared_ptr<bs::Wallet>&
      , QWidget* parent = nullptr);

   static std::shared_ptr<CreateTransactionDialogAdvanced>  CreateForCPFP(
        const std::shared_ptr<WalletsManager>&
      , const std::shared_ptr<SignContainer>&
      , const std::shared_ptr<bs::Wallet>&
      , const Tx &
      , QWidget* parent = nullptr);

public:
   CreateTransactionDialogAdvanced(const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &, bool loadFeeSuggestions, QWidget* parent);
   ~CreateTransactionDialogAdvanced() noexcept override = default;

   void preSetAddress(const QString& address);
   void preSetValue(const double value);

   void SetImportedTransactions(const std::vector<bs::wallet::TXSignRequest>& transactions);

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

   virtual QLabel *feePerByteLabel() const override;
   virtual QLabel *changeLabel() const override;

   void onTransactionUpdated() override;
   bs::Address getChangeAddress() const override;

   bool HaveSignedImportedTransaction() const override;

protected slots:
   void selectedWalletChanged(int currentIndex) override;

   void onAddressTextChanged(const QString& addressString);
   void onFeeSuggestionsLoaded(const std::map<unsigned int, float> &) override;
   void onXBTAmountChanged(const QString& text);

   void onSelectInputs();
   void onAddOutput();
   void onCreatePressed();
   void onImportPressed();

   void feeSelectionChanged(int currentIndex);
   void onManualFeeChanged(int fee);

   void onNewAddressSelectedForChange();
   void onExistingAddressSelectedForChange();

   void showContextMenu(const QPoint& point);
   void onRemoveOutput();

private:
   void clear() override;
   void initUI();

   void validateAddOutputButton();
   void validateCreateButton();

   void AddRecipient(const bs::Address &, double amount, bool isMax = false);

   void AddManualFeeEntries(float feePerByte, float totalFee);
   void SetMinimumFee(float totalFee, float feePerByte = 0);

   void SetFixedWallet(const std::string& walletId);
   void disableOutputsEditing();
   void disableInputSelection();
   void disableFeeChanging();
   void SetFixedChangeAddress(const QString& changeAddress);
   void setFixedFee(const int64_t& manualFee, bool perByte = false);
   void SetPredefinedFee(const int64_t& manualFee);
   void setUnchangeableTx();

   void RemoveOutputByRow(int row);

   void showExistingChangeAddress(bool show);

   void disableChangeAddressSelecting();

private:
   Ui::CreateTransactionDialogAdvanced*  ui_;

   bool     currentAddressValid_ = false;
   double   currentValue_ = 0;

   UsedInputsModel         *  usedInputsModel_ = nullptr;
   TransactionOutputsModel *  outputsModel_ = nullptr;

   bool        removeOutputEnabled_ = true;
   QMenu       contextMenu_;
   QAction *   removeOutputAction_ = nullptr;

   bool        changeAddressFixed_ = false;
   bs::Address selectedChangeAddress_;

   float       minTotalFee_ = 0;
   float       minFeePerByte_ = 0;
   const float minRelayFeePerByte_ = 5;

   bool        feeChangeDisabled_ = false;
};

#endif // __CREATE_TRANSACTION_DIALOG_ADVANCED_H__
