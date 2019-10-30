#include "AddressListModel.h"

#include <QApplication>
#include <QColor>

#include "Wallets/SyncWalletsManager.h"
#include "UiUtils.h"

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


AddressListModel::AddressListModel(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , QObject* parent, AddressType addrType)
   : QAbstractTableModel(parent)
   , walletsMgr_(walletsMgr)
   , addrType_(addrType)
   , processing_(false)
{
   if (walletsMgr_) {
      connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsReady, this
         , &AddressListModel::updateWallets);
      connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletChanged, this
         , &AddressListModel::updateData);
      connect(walletsMgr_.get(), &bs::sync::WalletsManager::blockchainEvent, this
         , &AddressListModel::updateWallets);
      connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletBalanceUpdated
         , this, &AddressListModel::updateData);
   }
}

bool AddressListModel::setWallets(const Wallets &wallets, bool force, bool filterBtcOnly)
{
   if ((wallets != wallets_) || (filterBtcOnly != filterBtcOnly_) || force) {
      wallets_ = wallets;
      filterBtcOnly_ = filterBtcOnly;
      updateWallets();
   }

   return true;
}

AddressListModel::AddressRow AddressListModel::createRow(const bs::Address &addr
   , const std::shared_ptr<bs::sync::Wallet> &wallet) const
{
   AddressRow row;

   row.wallet = wallet;
   row.address = addr;
   row.transactionCount = -1;
   row.balance = 0;

   if (wallet->type() == bs::core::wallet::Type::Authentication) {
      row.comment = tr("Authentication PubKey");
      const BinaryData rootId;
      row.displayedAddress = rootId.isNull() ? tr("empty") : QString::fromStdString(BtcUtils::base58_encode(rootId).toBinStr());
      row.isExternal = true;
   }
   else {
      row.comment = QString::fromStdString(wallet->getAddressComment(addr));
      row.displayedAddress = QString::fromStdString(addr.display());
      row.walletName = QString::fromStdString(wallet->shortName());
      row.walletId = QString::fromStdString(wallet->walletId());
      row.isExternal = wallet->isExternalAddress(addr);
   }
   row.wltType = wallet->type();
   return row;
}

void AddressListModel::updateWallets()
{
   updateData("");
}

void AddressListModel::updateData(const std::string &walletId)
{
   bool expected = false;
   bool desired = true;
   if (!std::atomic_compare_exchange_strong(&processing_, &expected, desired)) {
      return;
   }
   beginResetModel();
   addressRows_.clear();
   for (const auto &wallet : wallets_) {
      updateWallet(wallet);
   }
   endResetModel();
   updateWalletData();
   processing_.store(false);
}

void AddressListModel::updateWallet(const std::shared_ptr<bs::sync::Wallet> &wallet)
{
   if (filterBtcOnly_ && wallet->type() != bs::core::wallet::Type::Bitcoin) {
      return;
   }

   if (wallet->type() == bs::core::wallet::Type::Authentication) {
      const auto addr = bs::Address();
      auto row = createRow(addr, wallet);
      addressRows_.emplace_back(std::move(row));
   } else {
      if ((wallets_.size() > 1) && (wallet->type() == bs::core::wallet::Type::ColorCoin)) {
         return;  // don't populate PM addresses when multiple wallets selected
      }
      std::vector<bs::Address> addressList;
      switch (addrType_) {
      case AddressType::External:
         addressList = wallet->getExtAddressList();
         break;
      case AddressType::Internal:
         addressList = wallet->getIntAddressList();
         break;
      case AddressType::All:
      case AddressType::ExtAndNonEmptyInt:
      default:
         addressList = wallet->getUsedAddressList();
         break;
      }

      addressRows_.reserve(addressRows_.size() + addressList.size());

      for (size_t i = 0; i < addressList.size(); i++) {
         const auto &addr = addressList[i];

         auto row = createRow(addr, wallet);
         row.addrIndex = i;
         row.comment = QString::fromStdString(wallet->getAddressComment(addr));

         addressRows_.emplace_back(std::move(row));
      }
   }
}

void AddressListModel::updateWalletData()
{
   auto nbTxNs = std::make_shared<int>((int)addressRows_.size());
   auto nbBalances = std::make_shared<int>((int)addressRows_.size());

   auto addrTxNs = std::make_shared<std::vector<uint32_t>>();
   addrTxNs->resize(addressRows_.size());
   auto addrBalances = std::make_shared<std::vector<uint64_t>>();
   addrBalances->resize(addressRows_.size());

   for (size_t i = 0; i < addressRows_.size(); ++i) {
      // Callback for address's # of TXs.
      const auto &cbTxN = [this, addrTxNs, i, nbTxNs](uint64_t txn) {
         QMetaObject::invokeMethod(qApp, [this, handle = validityFlag_.handle(), i, nbTxNs, addrTxNs, txn] {
            --(*nbTxNs);
            if (i >= addressRows_.size()) {
               return;
            }
            (*addrTxNs)[i] = txn;

            // On the final address, set the TX count for all addresses and emit
            // any required signals.
            if (*nbTxNs <= 0) {
               for (size_t j = 0; j < std::min(addressRows_.size(), addrTxNs->size()); ++j) {
                  addressRows_[j].transactionCount = (*addrTxNs)[j];
               }
               emit dataChanged(index(0, ColumnTxCount)
                  , index(addressRows_.size() - 1, ColumnTxCount));
            }
         });
      };

      // Callback for address's balance.
      const auto &cbBalance = [this, handle = validityFlag_.handle(), addrBalances, i, nbBalances](std::vector<uint64_t> balances) {
         QMetaObject::invokeMethod(qApp, [this, handle, balances, addrBalances, i, nbBalances] {
            if (!handle.isValid()) {
               return;
            }
            --(*nbBalances);
            if (i >= addressRows_.size()) {
               return;
            }
            if (balances.size() == 3) {
               (*addrBalances)[i] = balances[0];
            }
            else {
               (*addrBalances)[i] = 0;
            }

            // On the final address, set the balance for all addresses and emit
            // any required signals.
            if (*nbBalances <= 0) {
               for (size_t j = 0;
                    j < std::min(addressRows_.size(), addrBalances->size());
                    ++j) {
                  addressRows_[j].balance = (*addrBalances)[j];
               }
               emit dataChanged(index(0, ColumnBalance)
                  , index(static_cast<int>(addressRows_.size()) - 1, ColumnBalance));
            }
         });
      };

      // Get an address's balance & # of TXs from Armory via the wallet.
      const auto &wallet = addressRows_[i].wallet;
      const auto &address = addressRows_[i].address;
      if (!wallet) {
         return;
      }
      wallet->onBalanceAvailable([wallet, address, cbTxN, cbBalance] {
         cbTxN(wallet->getAddrTxN(address));
         cbBalance(wallet->getAddrBalance(address));
      });
   }
}

void AddressListModel::removeEmptyIntAddresses()
{
   bool expected = false;
   bool desired = true;
   if (!std::atomic_compare_exchange_strong(&processing_, &expected, desired)) {
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
   return ColumnsNbSingle;
}

int AddressListModel::rowCount(const QModelIndex& parent) const
{
   if (parent.isValid()) {
      return 0;
   }

   if ((wallets_.size() == 1) && (wallets_[0]->type() == bs::core::wallet::Type::Authentication)) {
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
         if (row.wltType == bs::core::wallet::Type::ColorCoin) {
            if (wallets_.size() == 1) {
               return UiUtils::displayCCAmount(row.balance);
            }
            else {
               return {};
            }
         }
         else {
            return UiUtils::displayAmount(row.balance);
         }
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

   const auto row = addressRows_[index.row()];

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
         break;

      case Qt::TextColorRole:
/*         if (!row.isExternal) {
            return QColor(Qt::gray);
         }*/   // don't remove completely in case someone decides to revert back
         break;

      default:
         break;
   }

   return {};
}
