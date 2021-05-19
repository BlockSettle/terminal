/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CREATE_TRANSACTION_DIALOG_H__
#define __CREATE_TRANSACTION_DIALOG_H__

#include <memory>
#include <string>
#include <vector>

#include <QAction>
#include <QDialog>
#include <QMenu>
#include <QPoint>
#include <QString>

#include "Bip21Types.h"
#include "BSErrorCodeStrings.h"
#include "CoreWallet.h"
#include "SignContainer.h"
#include "UtxoReservationToken.h"
#include "ValidityFlag.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
   class UTXOReservationManager;
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

namespace UiUtils {
   enum WalletsTypes : int;
}


class CreateTransactionDialog : public QDialog
{
Q_OBJECT

public:
   CreateTransactionDialog(bool loadFeeSuggestions, uint32_t topBlock
      , const std::shared_ptr<spdlog::logger>&, QWidget* parent);
   ~CreateTransactionDialog() noexcept override;

   int SelectWallet(const std::string& walletId, UiUtils::WalletsTypes type);

   virtual void initUI() = 0;
   virtual bool switchModeRequested() const= 0;
   virtual std::shared_ptr<CreateTransactionDialog> SwitchMode() = 0;

   virtual void onAddresses(const std::string &walletId, const std::vector<bs::sync::Address>&) {}
   virtual void onAddressComments(const std::string& walletId
      , const std::map<bs::Address, std::string>&) {}
   virtual void onAddressBalances(const std::string& walletId
      , const std::vector<bs::sync::WalletBalanceData::AddressBalance>&) {}

   virtual void onWalletsList(const std::string &id, const std::vector<bs::sync::HDWalletData>&);
   void onFeeLevels(const std::map<unsigned int, float>&);
   void onUTXOs(const std::string& id, const std::string& walletId, const std::vector<UTXO>&);
   void onSignedTX(const std::string& id, BinaryData signedTX, bs::error::ErrorCode result);

signals:
   void feeLoadingCompleted(const std::map<unsigned int, float>&);  //deprecated
   void walletChanged();
   void needWalletsList(UiUtils::WalletsTypes, const std::string &id);
   void needFeeLevels(const std::vector<unsigned int>&);
   void needUTXOs(const std::string &id, const std::string& walletId
      , bool confOnly=false, bool swOnly=false);
   void needExtAddresses(const std::string& walletId);
   void needIntAddresses(const std::string& walletId);
   void needUsedAddresses(const std::string& walletId);
   void needAddrComments(const std::string& walletId, const std::vector<bs::Address>&);
   void needWalletBalances(const std::string& walletId);
   void needSignTX(const std::string& id, const bs::core::wallet::TXSignRequest &
      , bool keepDupRecips = false, SignContainer::TXSignMode mode = SignContainer::TXSignMode::Full);
   void needBroadcastZC(const std::string &id, const BinaryData &);
   void needSetTxComment(const std::string& walletId, const BinaryData& txHash, const std::string &comment);

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
   using CreateTransactionCb = std::function<void(bool success, const std::string &errorMsg
                                                  , const std::string& unsignedTx, uint64_t virtSize)>;
   void CreateTransaction(const CreateTransactionCb &cb);

   void showError(const QString &text, const QString &detailedText);

protected slots:
   virtual void onFeeSuggestionsLoaded(const std::map<unsigned int, float> &);
   virtual void feeSelectionChanged(int);
   virtual void selectedWalletChanged(int, bool resetInputs = false
      , const std::function<void()> &cbInputsReset = nullptr);
   virtual void onMaxPressed();
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result); //deprecated
   void updateCreateButtonText();
   void onSignerAuthenticated();

protected:
   void populateFeeList();

   bool createTransactionImpl();

   static bool canUseSimpleMode(const Bip21::PaymentRequestInfo& paymentInfo);

private:
   void loadFees();
   void populateWalletsList();
   void startBroadcasting();
   void stopBroadcasting();

protected:
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>   signContainer_;
   std::shared_ptr<TransactionData> transactionData_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<ApplicationSettings> applicationSettings_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;
   bs::UtxoReservationToken utxoRes_;
   uint32_t topBlock_;

   XbtAmountValidator * xbtValidator_ = nullptr;

   const bool     loadFeeSuggestions_;

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


   ValidityFlag validityFlag_;

   std::unordered_map<std::string, bs::sync::HDWalletData>  hdWallets_;

private:
   bs::core::wallet::TXSignRequest  txReq_;
};

#endif // __CREATE_TRANSACTION_DIALOG_H__
