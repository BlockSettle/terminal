#ifndef __CREATE_TRANSACTION_DIALOG_H__
#define __CREATE_TRANSACTION_DIALOG_H__

#include <vector>
#include <memory>
#include <string>
#include <QAction>
#include <QDialog>
#include <QMenu>
#include <QPoint>
#include <QString>
#include "MetaData.h"

class ArmoryConnection;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class RecipientWidget;
class SignContainer;
class TransactionData;
class TransactionOutputsModel;
class UsedInputsModel;
class WalletsManager;
class XbtAmountValidator;


class CreateTransactionDialog : public QDialog
{
Q_OBJECT

public:
   CreateTransactionDialog(const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , bool loadFeeSuggestions, QWidget* parent);
   ~CreateTransactionDialog() noexcept override;

   void SelectWallet(const std::string& walletId);
   void setOfflineDir(const QString &dir) { offlineDir_ = dir; }

protected:
   virtual void init();
   virtual void clear();
   void reject() override;

   virtual QComboBox *comboBoxWallets() const = 0;
   virtual QComboBox *comboBoxFeeSuggestions() const = 0;
   virtual QLineEdit *lineEditAddress() const = 0;
   virtual QLineEdit *lineEditAmount() const = 0;
   virtual QPushButton *pushButtonMax() const = 0;
   virtual QTextEdit *textEditComment() const = 0;
   virtual QCheckBox *checkBoxRBF() const = 0;
   virtual QLabel *labelBalance() const = 0;
   virtual QLabel *labelAmount() const = 0;
   virtual QLabel *labelTxInputs() const = 0;
   virtual QLabel *labelEstimatedFee() const = 0;
   virtual QLabel *labelTotalAmount() const = 0;
   virtual QLabel *labelTxSize() const = 0;
   virtual QPushButton *pushButtonCreate() const = 0;
   virtual QPushButton *pushButtonCancel() const = 0;

   virtual QLabel* feePerByteLabel() const { return nullptr; }
   virtual QLabel* changeLabel() const {return nullptr; }

   virtual bs::Address getChangeAddress() const = 0;

   virtual void onTransactionUpdated();

   virtual bool HaveSignedImportedTransaction() const { return false; }

   void updateCreateButtonText();

   std::vector<bs::wallet::TXSignRequest> ImportTransactions();
   bool BroadcastImportedTx();
   bool CreateTransaction();

   void showError(const QString &text, const QString &detailedText);

signals:
   void feeLoadingCompleted(const std::map<unsigned int, float> &);

protected slots:
   virtual void onFeeSuggestionsLoaded(const std::map<unsigned int, float> &);
   virtual void feeSelectionChanged(int);
   virtual void selectedWalletChanged(int);
   virtual void onMaxPressed();
   void onTXSigned(unsigned int id, BinaryData signedTX, std::string error);

private:
   void populateFeeList();
   void loadFees();
   void populateWalletsList();
   void startBroadcasting();
   void stopBroadcasting();

protected:
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<TransactionData> transactionData_;

   XbtAmountValidator * xbtValidator_ = nullptr;

   const bool     loadFeeSuggestions_;
   std::thread    feeUpdatingThread_;

   unsigned int   pendingTXSignId_ = 0;
   bool           broadcasting_ = false;
   uint64_t       originalFee_ = 0;

   QString        offlineDir_;
   BinaryData     importedSignedTX_;
};

#endif // __CREATE_TRANSACTION_DIALOG_H__
