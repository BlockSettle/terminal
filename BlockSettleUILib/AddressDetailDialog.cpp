#include "AddressDetailDialog.h"
#include "ui_AddressDetailDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>

#include "ArmoryConnection.h"
#include "MetaData.h"
#include "TransactionsViewModel.h"
#include "UiUtils.h"


class IncomingTransactionFilter : public QSortFilterProxyModel
{
public:
   QString address;

   IncomingTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {}
   bool filterAcceptsRow(int source_row, const QModelIndex &) const override {
      auto transactionAddress = sourceModel()->data(sourceModel()->index(source_row, static_cast<int>(TransactionsViewModel::Columns::Address))).toString();
      return transactionAddress == address;
   }
};

class OutgoingTransactionFilter : public QSortFilterProxyModel
{
public:
   QString address;

   OutgoingTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {}
   bool filterAcceptsRow(int source_row, const QModelIndex &) const override {
      auto transactionAddress = sourceModel()->data(sourceModel()->index(source_row, static_cast<int>(TransactionsViewModel::Columns::Address))).toString();
      if (transactionAddress.isEmpty()) {
         return false;
      }
      return transactionAddress != address;
   }
};

class AddressTransactionFilter : public QSortFilterProxyModel
{
public:
   AddressTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {}
   bool filterAcceptsColumn(int source_column, const QModelIndex & source_parent) const override
   {
      Q_UNUSED(source_parent);
      TransactionsViewModel::Columns col = static_cast<TransactionsViewModel::Columns>(source_column);
      return col != TransactionsViewModel::Columns::Wallet
         && col != TransactionsViewModel::Columns::Address
         && col != TransactionsViewModel::Columns::Comment
         && col != TransactionsViewModel::Columns::SendReceive
         && col != TransactionsViewModel::Columns::RbfFlag
         /*&& col != TransactionsViewModel::Columns::MissedBlocks*/;
   }
};


AddressDetailDialog::AddressDetailDialog(const bs::Address& address, const std::shared_ptr<bs::Wallet> &wallet
   , const std::shared_ptr<WalletsManager>& walletsManager, const std::shared_ptr<ArmoryConnection> &armory
   , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::AddressDetailDialog())
   , address_(address)
   , walletsManager_(walletsManager)
   , wallet_(wallet)
{
   setAttribute(Qt::WA_DeleteOnClose);
   ui_->setupUi(this);
   ui_->labelError->hide();

   connect(wallet_.get(), &bs::Wallet::addrBalanceReceived, this, &AddressDetailDialog::onAddrBalanceReceived);
   connect(wallet_.get(), &bs::Wallet::addrTxNReceived, this, &AddressDetailDialog::onAddrTxNReceived);

   wallet_->getAddrBalance(address);
   wallet_->getAddrTxN(address);

   auto copyButton = ui_->buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole);
   connect(copyButton, &QPushButton::clicked, this, &AddressDetailDialog::onCopyClicked);

   ui_->labelWallenName->setText(QString::fromStdString(wallet_->GetWalletName()));

   const auto addressString = address.display();
   ui_->labelAddress->setText(addressString);

   auto index = QString::fromStdString(wallet_->GetAddressIndex(address));
   if (index.length() > 64) {
      index = index.left(64);
   }
   ui_->labelAddrIndex->setText(index);

   const auto comment = wallet_->GetAddressComment(address);
   ui_->labelComment->setText(QString::fromStdString(comment));

   const auto &cbLedgerDelegate = [this, armory](AsyncClient::LedgerDelegate delegate) {
      // XXX - if armory is offline we need to reflect this in current dialog
      TransactionsViewModel* model = new TransactionsViewModel(armory, walletsManager_, delegate, this, wallet_);

      IncomingTransactionFilter* incomingFilter = new IncomingTransactionFilter(this);
      incomingFilter->address = address_.display();
      incomingFilter->setSourceModel(model);
      AddressTransactionFilter* inFilter = new AddressTransactionFilter(this);
      inFilter->setSourceModel(incomingFilter);
      ui_->inputAddressesWidget->setModel(inFilter);
      ui_->inputAddressesWidget->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date), Qt::DescendingOrder);

      OutgoingTransactionFilter* outgoingFilter = new OutgoingTransactionFilter(this);
      outgoingFilter->address = address_.display();
      outgoingFilter->setSourceModel(model);
      AddressTransactionFilter* outFilter = new AddressTransactionFilter(this);
      outFilter->setSourceModel(outgoingFilter);
      ui_->outputAddressesWidget->setModel(outFilter);
      ui_->outputAddressesWidget->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date), Qt::DescendingOrder);

      ui_->inputAddressesWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
      ui_->outputAddressesWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   };
   if (!armory->getLedgerDelegateForAddress(wallet_->GetWalletId(), address_, cbLedgerDelegate)) {
      ui_->labelError->setText(tr("Error loading address info"));
      onError();
   }

   ui_->labelQR->setPixmap(UiUtils::getQRCode(addressString, 128));
}

void AddressDetailDialog::onAddrBalanceReceived(const bs::Address &addr, std::vector<uint64_t> balance)
{
   if (addr != address_) {
      return;
   }
   ui_->labelBalance->setText((wallet_->GetType() == bs::wallet::Type::ColorCoin)
      ? UiUtils::displayCCAmount(balance[0]) : UiUtils::displayAmount(balance[0]));
}

void AddressDetailDialog::onAddrTxNReceived(const bs::Address &addr, uint32_t txn)
{
   if (addr != address_) {
      return;
   }
   ui_->labelTransactions->setText(QString::number(txn));
}

void AddressDetailDialog::onError()
{
   ui_->labelError->show();
   ui_->groupBoxIncoming->hide();
   ui_->groupBoxOutgoing->hide();
}

void AddressDetailDialog::onCopyClicked() const
{
   QApplication::clipboard()->setText(address_.display());
}
