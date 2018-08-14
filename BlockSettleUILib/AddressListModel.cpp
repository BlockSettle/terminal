#include "AddressListModel.h"
#include "WalletsManager.h"
#include "UiUtils.h"
#include <QtConcurrent/QtConcurrentRun>


bool AddressListModel::AddressRow::isMultiLineComment() const
{
   const auto commentLines = comment.split(QLatin1Char('\n'));
   if (commentLines.size() > 1) {
      return true;
   }
   return false;
}

QString AddressListModel::AddressRow::getComment() const
{
   const auto commentLines = comment.split(QLatin1Char('\n'));
   if (commentLines.size() <= 1) {
      return comment;
   }
   return (commentLines.at(0) + QLatin1String("..."));
}

QString AddressListModel::AddressRow::getAddress() const
{
   return UiUtils::displayAddress(displayedAddress);
}


AddressListModel::AddressListModel(std::shared_ptr<WalletsManager> walletsManager, QObject* parent
   , AddressType addrType)
   : QAbstractTableModel(parent)
   , addrType_(addrType)
   , processing_(false)
{
   connect(walletsManager.get(), &WalletsManager::walletsReady, this, &AddressListModel::updateData);
   connect(walletsManager.get(), &WalletsManager::walletChanged, this, &AddressListModel::updateData);
   connect(walletsManager.get(), &WalletsManager::blockchainEvent, this, &AddressListModel::updateData);
}

bool AddressListModel::setWallets(const Wallets &wallets)
{
   for (const auto &wallet : wallets_) {
      disconnect(wallet.get(), &bs::Wallet::addressAdded, this, &AddressListModel::updateData);
   }

   wallets_ = wallets;
   for (const auto &wallet : wallets_) {
      connect(wallet.get(), &bs::Wallet::addressAdded, this, &AddressListModel::updateData, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
   }

   updateData();
   return true;
}

AddressListModel::AddressRow AddressListModel::createRow(const bs::Address &addr, const std::shared_ptr<bs::Wallet> &wallet) const
{
   AddressRow row;

   row.wallet = wallet;
   row.address = addr;
   row.transactionCount = -1;
   row.balance = -1;

   if (wallet->GetType() == bs::wallet::Type::Authentication) {
      row.comment = tr("Authentication PubKey");
      const auto rootId = (wallet == nullptr) ? BinaryData() : wallet->getRootId();
      row.displayedAddress = rootId.isNull() ? tr("empty") : QString::fromStdString(BtcUtils::base58_encode(rootId).toBinStr());
      row.isExternal = true;
   }
   else {
      row.comment = QString::fromStdString(wallet->GetAddressComment(addr));
      row.displayedAddress = addr.display();
      row.walletName = QString::fromStdString(wallet->GetShortName());
      row.walletId = QString::fromStdString(wallet->GetWalletId());
      row.isExternal = wallet->IsExternalAddress(addr);
   }
   row.wltType = wallet->GetType();
   return row;
}

void AddressListModel::updateData()
{
   beginResetModel();

   addressRows_.clear();

   for (const auto &wallet : wallets_) {
      updateWallet(wallet);
   }

   endResetModel();
   updateWalletData();
}

void AddressListModel::updateWallet(const std::shared_ptr<bs::Wallet> &wallet)
{
   bool expected = false;
   if (!std::atomic_compare_exchange_strong(&processing_, &expected, true)) {
      return;
   }

   if (wallet->GetType() == bs::wallet::Type::Authentication) {
      const auto addr = bs::Address();
      auto row = createRow(addr, wallet);
      addressRows_.push_back(std::move(row));
   } else {
      std::vector<bs::Address> addressList;
      switch (addrType_) {
      case AddressType::External:
         addressList = wallet->GetExtAddressList();
         break;
      case AddressType::Internal:
         addressList = wallet->GetIntAddressList();
         break;
      case AddressType::All:
      case AddressType::ExtAndNonEmptyInt:
      default:
         addressList = wallet->GetUsedAddressList();
         break;
      }

      addressRows_.reserve(addressRows_.size() + addressList.size());

      for (size_t i = 0; i < addressList.size(); i++) {
         const auto &addr = addressList[i];

         auto row = createRow(addr, wallet);
         row.addrIndex = i;
         row.comment = QString::fromStdString(wallet->GetAddressComment(addr));

         addressRows_.push_back(std::move(row));
      }
      processing_.store(false);
   }
}

void AddressListModel::updateWalletData()
{
   for (size_t i = 0; i < addressRows_.size(); ++i) {
      auto &addrRow = addressRows_[i];
      const auto &cbTxN = [this, &addrRow, i](uint32_t txn) {
         if (i >= addressRows_.size()) {
            return;
         }
         addrRow.transactionCount = txn;
         emit dataChanged(index(i, ColumnTxCount), index(i, ColumnTxCount));
      };

      const auto &cbBalance = [this, &addrRow, i](std::vector<uint64_t> balances) {
         if (i >= addressRows_.size()) {
            return;
         }
         addrRow.balance = balances[0];
         emit dataChanged(index(i, ColumnBalance), index(i, ColumnBalance));
      };
      addrRow.wallet->getAddrTxN(addrRow.address, cbTxN);
      addrRow.wallet->getAddrBalance(addrRow.address, cbBalance);
   }

   // XXX when all tx counts and balance loaded
   // when row created tx count and balance are set to -1
   // if (addrType_ == AddressType::ExtAndNonEmptyInt) {
   //    QMetaObject::invokeMethod(this, "removeEmptyIntAddresses");
   // }
}

void AddressListModel::removeEmptyIntAddresses()
{
   bool expected = false;
   if (!std::atomic_compare_exchange_strong(&processing_, &expected, true)) {
      return;
   }

   std::set<int> indicesToRemove;
   for (size_t i = 0; i < addressRows_.size(); ++i) {
      const auto &row = addressRows_[i];
      if (!row.isExternal && !row.transactionCount && !row.balance) {
         indicesToRemove.insert(i);
      }
   }
   unsigned int nbRemoved = 0;
   for (auto idx : indicesToRemove) {
      idx -= nbRemoved;
      if (addressRows_.size() <= idx) {
         processing_.store(false);
         return;
      }
      beginRemoveRows(QModelIndex(), idx, idx);
      addressRows_.erase(addressRows_.begin() + idx);
      endRemoveRows();
      ++nbRemoved;
   }
   processing_.store(false);
}

int AddressListModel::columnCount(const QModelIndex &) const
{
   if (wallets_.empty()) {
      return 0;
   }
   if (wallets_.size() == 1) {
      return ColumnsNbSingle;
   }
   return ColumnsNbMultiple;
}

int AddressListModel::rowCount(const QModelIndex& parent) const
{
   if (parent.isValid()) {
      return 0;
   }

   if ((wallets_.size() == 1) && (wallets_[0]->GetType() == bs::wallet::Type::Authentication)) {
      return 1;
   }

   return (int) addressRows_.size();
}

QVariant AddressListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
   {
      switch (section)
      {
         case ColumnAddress:
            return tr("Address");

         case ColumnTxCount:
            return tr("#Tx");

         case ColumnBalance:
            return tr("Balance");

         case ColumnComment:
            return tr("Comment");

         case ColumnWallet:
            return tr("Wallet");

         default: break;
      }
   }

   return {};
}

QVariant AddressListModel::dataForRow(const AddressListModel::AddressRow &row, int column) const
{
   switch (column)
   {
      case AddressListModel::ColumnAddress:
         return row.getAddress();
      case AddressListModel::ColumnBalance:
         return (row.wltType == bs::wallet::Type::ColorCoin) ? UiUtils::displayCCAmount(row.balance)
            : UiUtils::displayAmount(row.balance);
      case AddressListModel::ColumnTxCount:
         return row.transactionCount;
      case AddressListModel::ColumnComment:
         return row.getComment();
      case AddressListModel::ColumnWallet:
         if (row.walletName.isEmpty()) {
            return {};
         }
         return row.walletName;
      default:
         return {};
   }
}

QVariant AddressListModel::data(const QModelIndex& index, int role) const
{
   if (role == Qt::TextAlignmentRole) {
      if (index.column() == ColumnBalance) {
         return Qt::AlignRight;
      }
      return Qt::AlignLeading;
   }

   if (index.row() >= addressRows_.size()) {
      return {};
   }

   const auto& row = addressRows_[index.row()];

   switch (role) {
      case Qt::DisplayRole:
         return dataForRow(row, index.column());

      case WalletIdRole:
         return row.walletId;

      case AddrIndexRole:
         return static_cast<unsigned int>(row.addrIndex);

      case IsExternalRole:
         return row.isExternal;

      case AddressRole:
         return row.displayedAddress;

      case Qt::ToolTipRole:
         if ((index.column() == ColumnComment) && row.isMultiLineComment()) {
            return row.comment;
         }
         break;

      case SortRole:
         if (index.column() == ColumnBalance) {
            return QVariant::fromValue<qlonglong>(row.balance);
         }
         else {
            return dataForRow(row, index.column());
         }

      default:
         break;
   }

   return {};
}
