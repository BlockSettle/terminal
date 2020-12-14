/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ADDRESSLISTMODEL_H
#define ADDRESSLISTMODEL_H

#include <map>
#include <memory>
#include <QAbstractTableModel>
#include "CoreWallet.h"
#include "SignerDefs.h"
#include "ValidityFlag.h"

namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}

// Table for address detail list. Used for address & wallet detail widgets. Not
// used for blockchain explorer address widget.
class AddressListModel : public QAbstractTableModel
{
   Q_OBJECT

public:
   struct AddressRow
   {
      bs::Address address;
      QByteArray bytes;
      int transactionCount = 0;
      uint64_t balance = 0;
      QString  comment;
      QString  displayedAddress;
      QString  walletName;
      QString  walletId;
      QString  index;
      int      addrIndex = 0;
      bs::core::wallet::Type wltType = bs::core::wallet::Type::Unknown;
      bool     isExternal;

      bool isMultiLineComment() const;
      QString getComment() const;
      QString getAddress() const;

      bool operator==(const AddressRow& other) const;
   };

   enum Columns
   {
      ColumnAddress = 0,
      ColumnTxCount,
      ColumnBalance,
      ColumnComment,
      ColumnsNbSingle,
      ColumnWallet = ColumnsNbSingle,
      ColumnsNbMultiple
   };

   enum Role
   {
      SortRole = Qt::UserRole,
      WalletIdRole,
      AddrIndexRole,
      AddressRole,
      AddressCommentRole,
      IsExternalRole,
      AddressIndexRole,
      WalletTypeRole,
      WalletNameRole,
      TxNRole,
      BalanceRole
   };

   enum AddressType {
      External = 1,
      Internal = 2,
      All = 3,
      ExtAndNonEmptyInt = 4
   };

   typedef std::vector<bs::sync::WalletInfo> Wallets;

   [[deprecated]] AddressListModel(const std::shared_ptr<bs::sync::WalletsManager> &, QObject* parent
      , AddressType addrType = AddressType::All);
   AddressListModel(QObject* parent, AddressType addrType = AddressType::All);
   ~AddressListModel() noexcept = default;

   int rowCount(const QModelIndex & parent) const override;
   int columnCount(const QModelIndex & parent) const override;
   QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

   void setWallets(const Wallets &, bool force, bool filterBtcOnly);
   void onAddresses(const std::string &walletId, const std::vector<bs::sync::Address> &);
   void onAddressComments(const std::string &walletId
      , const std::map<bs::Address, std::string> &);
   void onAddressBalances(const std::string &walletId
      , const std::vector<bs::sync::WalletBalanceData::AddressBalance> &);

signals:
   void needExtAddresses(const std::string &walletId);
   void needIntAddresses(const std::string &walletId);
   void needUsedAddresses(const std::string &walletId);
   void needAddrComments(const std::string &walletId, const std::vector<bs::Address> &);

private slots:
   void updateWallets();   // deprecated
//   void updateData(const std::string &walletId);
   void removeEmptyIntAddresses();  // deprecated

private:
   [[deprecated]] std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   Wallets                    wallets_;
   std::vector<AddressRow>    addressRows_;
   const AddressType          addrType_;
   std::map<BinaryData, int>  indexByAddr_;

   struct AddrBalance {
      uint64_t    balance;
      uint32_t    txn;
   };
   std::unordered_map<std::string, std::map<BinaryData, AddrBalance>>   pooledBalances_;

   std::atomic_bool           processing_;
   bool filterBtcOnly_{false};
   ValidityFlag validityFlag_;

private:
   void updateWallet(const bs::sync::WalletInfo &wallet);
   void updateWalletData();
   AddressRow createRow(const bs::Address &, const bs::sync::WalletInfo &) const;
   QVariant dataForRow(const AddressListModel::AddressRow &row, int column) const;
};

#endif // ADDRESSLISTMODEL_H
