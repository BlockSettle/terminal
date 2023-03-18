/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef WALLET_BALANCES_MODEL_H
#define WALLET_BALANCES_MODEL_H

#include <memory>
#include <QAbstractTableModel>
#include <QColor>
#include <QObject>
#include <QVariant>

namespace spdlog {
   class logger;
}

namespace WalletBalance {
   Q_NAMESPACE
   enum WalletBalancesRoles {
      NameRole = Qt::DisplayRole,
      IdRole = Qt::UserRole,
      TotalRole,
      ConfirmedRole,
      UnconfirmedRole,
      NbAddrRole
   };
   Q_ENUM_NS(WalletBalancesRoles)
}

class WalletBalancesModel : public QAbstractTableModel
{
   Q_OBJECT
   Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
   Q_PROPERTY(int selectedWallet READ selectedWallet WRITE setSelectedWallet NOTIFY changed)
   Q_PROPERTY(QString confirmedBalance   READ confirmedBalance NOTIFY changed)
   Q_PROPERTY(QString unconfirmedBalance READ unconfirmedBalance NOTIFY changed)
   Q_PROPERTY(QString totalBalance       READ totalBalance NOTIFY changed)
   Q_PROPERTY(QString numberAddresses   READ numberAddresses NOTIFY changed)

public:
   WalletBalancesModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override { return 1; }   // only wallet names
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   QStringList wallets() const;
   void clear();
   struct Wallet {
      std::string walletId;
      std::string walletName;
   };
   void addWallet(const Wallet&);
   void deleteWallet(const std::string& walletId);

   struct Balance {
      double   confirmed{ 0 };
      double   unconfirmed{ 0 };
      double   total{ 0 };
      uint32_t nbAddresses{ 0 };
   };
   void setWalletBalance(const std::string& walletId, const Balance&);

   void setSelectedWallet(int index);
   int selectedWallet() const;
   QString confirmedBalance() const;
   QString unconfirmedBalance() const;
   QString totalBalance() const;
   QString numberAddresses() const;

signals:
   void changed();
   void rowCountChanged();

private:
   using FieldFunc = std::function<QString(const Balance&)>;
   QString getBalance(const std::string& walletId, const FieldFunc&) const;

private:
   int selectedWallet_;
   std::shared_ptr<spdlog::logger>  logger_;
   std::vector<Wallet>  wallets_;
   std::unordered_map<std::string, Balance>  balances_;  //key: walletId
};

#endif	// WALLET_BALANCES_MODEL_H
