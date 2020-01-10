/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
#include "BSErrorCodeStrings.h"
#include "CoreWallet.h"
#include "ValidityFlag.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class ApplicationSettings;
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
class XbtAmountValidator;


class CreateTransactionDialog : public QDialog
{
Q_OBJECT

public:
   CreateTransactionDialog(const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , bool loadFeeSuggestions, const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ApplicationSettings> &applicationSettings
      , QWidget* parent);
   ~CreateTransactionDialog() noexcept override;

   int SelectWallet(const std::string& walletId);

protected:
   virtual void init();
   virtual void clear();
   void reject() override;
   void closeEvent(QCloseEvent *e) override;

   virtual QComboBox *comboBoxWallets() const = 0;
   virtual QComboBox *comboBoxFeeSuggestions() const = 0;
   virtual QLineEdit *lineEditAddress() const = 0;
   virtual QLineEdit *lineEditAmount() const = 0;
   virtual QPushButton *pushButtonMax() const = 0;
   virtual QTextEdit *textEditComment() const = 0;
   virtual QCheckBox *checkBoxRBF() const = 0;
   virtual QLabel *labelBalance() const = 0;
   virtual QLabel *labelAmount() const = 0;
   virtual QLabel *labelTXAmount() const = 0;
   virtual QLabel *labelTxInputs() const = 0;
   virtual QLabel *labelTxOutputs() const = 0;
   virtual QLabel *labelEstimatedFee() const = 0;
   virtual QLabel *labelTotalAmount() const = 0;
   virtual QLabel *labelTxSize() const = 0;
   virtual QPushButton *pushButtonCreate() const = 0;
   virtual QPushButton *pushButtonCancel() const = 0;

   virtual QLabel* feePerByteLabel() const { return nullptr; }
   virtual QLabel* changeLabel() const {return nullptr; }

   using AddressCb = std::function<void(const bs::Address&)>;
   virtual void getChangeAddress(AddressCb) const = 0;

   virtual void onTransactionUpdated();

   virtual bool HaveSignedImportedTransaction() const { return false; }

   std::vector<bs::core::wallet::TXSignRequest> ImportTransactions();
   bool BroadcastImportedTx();
   void CreateTransaction(std::function<void(bool result)> cb);

   void showError(const QString &text, const QString &detailedText);

signals:
   void feeLoadingCompleted(const std::map<unsigned int, float> &);

protected slots:
   virtual void onFeeSuggestionsLoaded(const std::map<unsigned int, float> &);
   virtual void feeSelectionChanged(int);
   virtual void selectedWalletChanged(int, bool resetInputs = false
      , const std::function<void()> &cbInputsReset = nullptr);
   virtual void onMaxPressed();
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result);
   void updateCreateButtonText();
   void onSignerAuthenticated();

protected:
   void populateFeeList();

private:
   void loadFees();
   void populateWalletsList();
   void startBroadcasting();
   void stopBroadcasting();
   bool createTransactionImpl(const bs::Address &changeAddress);

protected:
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>   signContainer_;
   std::shared_ptr<TransactionData> transactionData_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<ApplicationSettings> applicationSettings_;

   XbtAmountValidator * xbtValidator_ = nullptr;

   const bool     loadFeeSuggestions_;
   std::thread    feeUpdatingThread_;

   unsigned int   pendingTXSignId_ = 0;
   bool           broadcasting_ = false;
   uint64_t       originalFee_ = 0;
   float          originalFeePerByte_ = 0.0f;

   BinaryData     importedSignedTX_;

   bool isCPFP_ = false;
   bool isRBF_ = false;

   float       minTotalFee_ = 0;
   float       minFeePerByte_ = 0;
   float       advisedFeePerByte_ = 0;
   float       advisedTotalFee_ = 0;
   float       addedFee_ = 0;
   const float minRelayFeePerByte_ = 5;

   ValidityFlag validityFlag_;

private:
   bs::core::wallet::TXSignRequest  txReq_;
};

#endif // __CREATE_TRANSACTION_DIALOG_H__
