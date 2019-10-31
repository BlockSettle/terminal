#ifndef ADDRESSLISTMODEL_H
#define ADDRESSLISTMODEL_H

#include <map>
#include <memory>
#include <QAbstractTableModel>
#include "CoreWallet.h"
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
      std::shared_ptr<bs::sync::Wallet> wallet;
      bs::Address address;
      QByteArray bytes;
      int transactionCount = 0;
      uint64_t balance = 0;
      QString  comment;
      QString  displayedAddress;
      QString  walletName;
      QString  walletId;
      size_t   addrIndex = 0;
      bs::core::wallet::Type wltType = bs::core::wallet::Type::Unknown;
      bool     isExternal;

      bool isMultiLineComment() const;
      QString getComment() const;
      QString getAddress() const;
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
      IsExternalRole
   };

   enum AddressType {
      External = 1,
      Internal = 2,
      All = 3,
      ExtAndNonEmptyInt = 4
   };

   typedef std::vector<std::shared_ptr<bs::sync::Wallet>>   Wallets;

   AddressListModel(const std::shared_ptr<bs::sync::WalletsManager> &, QObject* parent
      , AddressType addrType = AddressType::All);
   ~AddressListModel() noexcept = default;

   int rowCount(const QModelIndex & parent) const override;
   int columnCount(const QModelIndex & parent) const override;
   QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

   bool setWallets(const Wallets &, bool force, bool filterBtcOnly);

private slots:
   void updateWallets();
   void updateData(const std::string &walletId);
   void removeEmptyIntAddresses();

private:
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   Wallets                    wallets_;
   std::vector<AddressRow>    addressRows_;
   const AddressType          addrType_;

   std::atomic_bool           processing_;
   bool filterBtcOnly_{false};
   ValidityFlag validityFlag_;

private:
   void updateWallet(const std::shared_ptr<bs::sync::Wallet> &);
   void updateWalletData();
   AddressRow createRow(const bs::Address &, const std::shared_ptr<bs::sync::Wallet> &) const;
   QVariant dataForRow(const AddressListModel::AddressRow &row, int column) const;
};

#endif // ADDRESSLISTMODEL_H
