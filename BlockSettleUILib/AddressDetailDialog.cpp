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
   IncomingTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {}

   bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
   {
      const auto txModel = qobject_cast<TransactionsViewModel *>(sourceModel());
      if (!txModel) {
         return false;
      }
      const auto &index = txModel->index(source_row, 0, source_parent);
      const auto &txItem = txModel->getItem(index);
      return (txItem.isSet() && (txItem.txEntry.value > 0));
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
      return (txItem.isSet() && (txItem.txEntry.value < 0));
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
                                     , const std::shared_ptr<bs::Wallet> &wallet
                         , const std::shared_ptr<WalletsManager>& walletsManager
                               , const std::shared_ptr<ArmoryConnection> &armory
                                 , const std::shared_ptr<spdlog::logger> &logger
                                         , QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::AddressDetailDialog())
   , address_(address)
   , walletsManager_(walletsManager)
   , armory_(armory)
   , wallet_(wallet)
   , logger_(logger)
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
      const auto &cbLedgerDelegate = [this, armory](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
         initModels(delegate);
      };
      if (!armory->getLedgerDelegateForAddress(wallet_->GetWalletId(), address_, cbLedgerDelegate, this)) {
         ui_->labelError->setText(tr("Error loading address info"));
         onError();
      }
   }

   const QString addrURI = QLatin1String("bitcoin:") + addressString;
   ui_->labelQR->setPixmap(UiUtils::getQRCode(addrURI, 128));

   ui_->inputAddressesWidget->setContextMenuPolicy(Qt::CustomContextMenu);
   ui_->outputAddressesWidget->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(ui_->inputAddressesWidget, &QTreeView::customContextMenuRequested,
      this, &AddressDetailDialog::onInputAddrContextMenu);
   connect(ui_->outputAddressesWidget, &QTreeView::customContextMenuRequested,
      this, &AddressDetailDialog::onOutputAddrContextMenu);
}

AddressDetailDialog::~AddressDetailDialog() = default;

void AddressDetailDialog::initModels(const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate)
{
   TransactionsViewModel* model = new TransactionsViewModel(armory_
                                                            , walletsManager_
                                                            , delegate
                                                            , logger_
                                                            , this
                                                            , wallet_);

   IncomingTransactionFilter* incomingFilter = new IncomingTransactionFilter(this);
   incomingFilter->setSourceModel(model);
   AddressTransactionFilter* inFilter = new AddressTransactionFilter(this);
   inFilter->setSourceModel(incomingFilter);
   ui_->inputAddressesWidget->setModel(inFilter);
   ui_->inputAddressesWidget->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date), Qt::DescendingOrder);

   OutgoingTransactionFilter* outgoingFilter = new OutgoingTransactionFilter(this);
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
