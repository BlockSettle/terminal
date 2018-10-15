#include "AddressDetailDialog.h"
#include "ui_AddressDetailDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QAction>

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
   , armory_(armory)
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

   ui_->inputAddressesWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->outputAddressesWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   if (armory_->state() != ArmoryConnection::State::Ready) {
      ui_->labelError->setText(tr("Armory is not connected"));
      onError();
   }
   else {
      const auto &cbLedgerDelegate = [this, armory](AsyncClient::LedgerDelegate delegate) {
         QMetaObject::invokeMethod(this, "initModels", Qt::QueuedConnection
            , Q_ARG(AsyncClient::LedgerDelegate, delegate));
      };
      if (!armory->getLedgerDelegateForAddress(wallet_->GetWalletId(), address_, cbLedgerDelegate)) {
         ui_->labelError->setText(tr("Error loading address info"));
         onError();
      }
   }

   ui_->labelQR->setPixmap(UiUtils::getQRCode(addressString, 128));

   ui_->inputAddressesWidget->setContextMenuPolicy(Qt::CustomContextMenu);
   ui_->outputAddressesWidget->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(ui_->inputAddressesWidget, &QTreeView::customContextMenuRequested,
      this, &AddressDetailDialog::onInputAddrContextMenu);
   connect(ui_->outputAddressesWidget, &QTreeView::customContextMenuRequested,
      this, &AddressDetailDialog::onOutputAddrContextMenu);
}

AddressDetailDialog::~AddressDetailDialog() = default;

void AddressDetailDialog::initModels(AsyncClient::LedgerDelegate delegate)
{
   TransactionsViewModel* model = new TransactionsViewModel(armory_, walletsManager_, delegate, this, wallet_);
   model->init();

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
