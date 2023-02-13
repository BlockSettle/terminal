/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AddressDetailDialog.h"
#include "ui_AddressDetailDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QAction>

#include "ArmoryConnection.h"
#include "HDPath.h"
#include "TransactionsViewModel.h"
#include "UiUtils.h"
#include "Wallets/SyncWallet.h"


class IncomingTransactionFilter : public QSortFilterProxyModel
{
public:
   IncomingTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {}

   bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
   {
      const auto txModel = qobject_cast<TransactionsViewModel *>(sourceModel());
      if (!txModel) {
         return false;
      }
      const auto &index = txModel->index(source_row, 0, source_parent);
      const auto &txItem = txModel->getItem(index);
      return (txItem && (txItem->txEntry.value > 0));
   }
};

class OutgoingTransactionFilter : public QSortFilterProxyModel
{
public:
   OutgoingTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {}

   bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
   {
      const auto txModel = qobject_cast<TransactionsViewModel *>(sourceModel());
      if (!txModel) {
         return false;
      }
      const auto &index = txModel->index(source_row, 0, source_parent);
      const auto &txItem = txModel->getItem(index);
      return (txItem && (txItem->txEntry.value < 0));
   }
};

class AddressTransactionFilter : public QSortFilterProxyModel
{
public:
   enum class Columns {
      Date = 0,
      Amount,
      Status,
      TxHash,
      last = TxHash
   };

   AddressTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {}
   bool filterAcceptsColumn(int source_column, const QModelIndex & source_parent) const override
   {
      Q_UNUSED(source_parent);
      TransactionsViewModel::Columns col = static_cast<TransactionsViewModel::Columns>(source_column);
      return col != TransactionsViewModel::Columns::Wallet
         && col != TransactionsViewModel::Columns::Address
         && col != TransactionsViewModel::Columns::Comment
         && col != TransactionsViewModel::Columns::SendReceive
         /*&& col != TransactionsViewModel::Columns::MissedBlocks*/;
   }
};


AddressDetailDialog::AddressDetailDialog(const bs::Address& address
   , const std::shared_ptr<spdlog::logger> &logger, bs::core::wallet::Type wltType
   , uint64_t balance, uint32_t txn, const QString &walletName
   , const std::string &addrIndex, const std::string &comment, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::AddressDetailDialog())
   , address_(address)
   , logger_(logger)
{
   ui_->setupUi(this);
   ui_->labelError->hide();

   setBalance(balance, wltType);
   onAddrTxNReceived(txn);

   auto copyButton = ui_->buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole);
   connect(copyButton, &QPushButton::clicked, this, &AddressDetailDialog::onCopyClicked);

   ui_->labelWalletName->setText(walletName);

   const auto addressString = QString::fromStdString(address.display());
   ui_->labelAddress->setText(addressString);

   const auto path = bs::hd::Path::fromString(addrIndex);
   QString index;
   if (path.length() != 2) {
      index = QString::fromStdString(addrIndex);
   } else {
      const auto lastIndex = QString::number(path.get(-1));
      switch (path.get(-2)) {
      case 0:
         index = tr("External/%1").arg(lastIndex);
         break;
      case 1:
         index = tr("Internal/%1").arg(lastIndex);
         break;
      default:
         index = tr("Unknown/%1").arg(lastIndex);
      }
   }
   if (index.length() > 64) {
      index = index.left(64);
   }
   ui_->labelAddrIndex->setText(index);

   ui_->labelComment->setText(QString::fromStdString(comment));

   ui_->inputAddressesWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->outputAddressesWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   const QString addrURI = QLatin1String("bitcoin:") + addressString;
   ui_->labelQR->setPixmap(UiUtils::getQRCode(addrURI, 128));

   ui_->inputAddressesWidget->setContextMenuPolicy(Qt::CustomContextMenu);
   ui_->outputAddressesWidget->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(ui_->inputAddressesWidget, &QTreeView::customContextMenuRequested,
      this, &AddressDetailDialog::onInputAddrContextMenu);
   connect(ui_->outputAddressesWidget, &QTreeView::customContextMenuRequested,
      this, &AddressDetailDialog::onOutputAddrContextMenu);

   model_ = new TransactionsViewModel(logger_, this);
   connect(model_, &TransactionsViewModel::needTXDetails, this, &AddressDetailDialog::needTXDetails);

   IncomingTransactionFilter* incomingFilter = new IncomingTransactionFilter(this);
   incomingFilter->setSourceModel(model_);
   AddressTransactionFilter* inFilter = new AddressTransactionFilter(this);
   inFilter->setSourceModel(incomingFilter);
   ui_->inputAddressesWidget->setModel(inFilter);
   ui_->inputAddressesWidget->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date), Qt::DescendingOrder);

   OutgoingTransactionFilter* outgoingFilter = new OutgoingTransactionFilter(this);
   outgoingFilter->setSourceModel(model_);
   AddressTransactionFilter* outFilter = new AddressTransactionFilter(this);
   outFilter->setSourceModel(outgoingFilter);
   ui_->outputAddressesWidget->setModel(outFilter);
   ui_->outputAddressesWidget->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date), Qt::DescendingOrder);
}

AddressDetailDialog::~AddressDetailDialog() = default;

void AddressDetailDialog::onNewBlock(unsigned int blockNum)
{
   model_->onNewBlock(blockNum);
}

void AddressDetailDialog::onLedgerEntries(uint32_t curBlock
   , const std::vector<bs::TXEntry> &entries)
{
   model_->onLedgerEntries({}, 0, 0, curBlock, entries);
}

void AddressDetailDialog::onTXDetails(const std::vector<bs::sync::TXWalletDetails> &details)
{
   model_->onTXDetails(details);
}

void AddressDetailDialog::setBalance(uint64_t balance, bs::core::wallet::Type wltType)
{
   ui_->labelBalance->setText((wltType == bs::core::wallet::Type::ColorCoin)
      ? UiUtils::displayCCAmount(balance) : UiUtils::displayAmount(balance));
}

void AddressDetailDialog::onAddrTxNReceived(uint32_t txn)
{
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
   QApplication::clipboard()->setText(QString::fromStdString(address_.display()));
}

void AddressDetailDialog::onInputAddrContextMenu(const QPoint &pos)
{
   QMenu menu(ui_->inputAddressesWidget);

   menu.addAction(tr("Copy Hash"), [this] () {
      const auto current = ui_->inputAddressesWidget->currentIndex();
      if (current.isValid() && ui_->inputAddressesWidget->model()) {
         QApplication::clipboard()->setText(ui_->inputAddressesWidget->model()->data(
            ui_->inputAddressesWidget->model()->index(current.row(),
               static_cast<int>(AddressTransactionFilter::Columns::TxHash),
               current.parent())).toString());
      }
   });

   menu.exec(ui_->inputAddressesWidget->mapToGlobal(pos));
}

void AddressDetailDialog::onOutputAddrContextMenu(const QPoint &pos)
{
   QMenu menu(ui_->outputAddressesWidget);

   menu.addAction(tr("Copy Hash"), [this] () {
      const auto current = ui_->outputAddressesWidget->currentIndex();
      if (current.isValid() && ui_->outputAddressesWidget->model()) {
         QApplication::clipboard()->setText(ui_->outputAddressesWidget->model()->data(
            ui_->outputAddressesWidget->model()->index(current.row(),
               static_cast<int>(AddressTransactionFilter::Columns::TxHash),
               current.parent())).toString());
      }
   });

   menu.exec(ui_->outputAddressesWidget->mapToGlobal(pos));
}
