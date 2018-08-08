#ifndef ADDRESSLISTMODEL_H
#define ADDRESSLISTMODEL_H

#include "MetaData.h"
#include <map>
#include <memory>
#include <QAbstractTableModel>

class WalletsManager;
namespace bs {
   class Wallet;
}

class AddressListModel : public QAbstractTableModel
{
   Q_OBJECT

public:
   struct AddressRow
   {
      std::shared_ptr<bs::Wallet> wallet;
      bs::Address address;
      QByteArray bytes;
      int transactionCount = 0;
      uint64_t balance = 0;
      QString  comment;
      QString  displayedAddress;
      QString  walletName;
      QString  walletId;
      size_t   addrIndex = 0;
      bs::wallet::Type wltType = bs::wallet::Type::Unknown;
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
      IsExternal
   };

   enum AddressType {
      External = 1,
      Internal = 2,
      All = 3,
      ExtAndNonEmptyInt = 4
   };

   typedef std::vector<std::shared_ptr<bs::Wallet>>   Wallets;

   AddressListModel(std::shared_ptr<WalletsManager> walletsManager, QObject* parent
      , AddressType addrType = AddressType::All);
   ~AddressListModel() override;

   int rowCount(const QModelIndex & parent) const override;
   int columnCount(const QModelIndex & parent) const override;
   QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

   bool setWallets(const Wallets &);

private slots:
   void updateData();
   void removeEmptyIntAddresses();

private:
   Wallets                    wallets_;
   std::vector<AddressRow>    addressRows_;
   const AddressType          addrType_;

   std::thread                updateThread_;
   std::atomic_bool           stopped_;
   std::atomic_bool           processing_;

private:
   void updateWallet(const std::shared_ptr<bs::Wallet> &);
   void updateWalletData();
   void stopUpdateThread();
   AddressRow createRow(const bs::Address &, const std::shared_ptr<bs::Wallet> &) const;
   QVariant dataForRow(const AddressListModel::AddressRow &row, int column) const;
};

#endif // ADDRESSLISTMODEL_H
