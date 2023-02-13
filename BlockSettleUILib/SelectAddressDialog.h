/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SELECT_ADDRESS_DIALOG_H__
#define __SELECT_ADDRESS_DIALOG_H__

#include <QDialog>
#include <memory>

#include "Address.h"
#include "AddressListModel.h"
#include "Wallets/SyncHDGroup.h"


namespace Ui {
    class SelectAddressDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}


class SelectAddressDialog : public QDialog
{
Q_OBJECT

public:
   SelectAddressDialog(QWidget* parent
      , AddressListModel::AddressType addrType = AddressListModel::AddressType::All);
   ~SelectAddressDialog() override;

   bs::Address getSelectedAddress() const;
   void setWallets(const AddressListModel::Wallets&);

   void onAddresses(const std::string& walletId, const std::vector<bs::sync::Address>&);
   void onAddressComments(const std::string& walletId
      , const std::map<bs::Address, std::string>&);
   void onAddressBalances(const std::string& walletId
      , const std::vector<bs::sync::WalletBalanceData::AddressBalance>&);

signals:
   void needExtAddresses(const std::string& walletId);
   void needIntAddresses(const std::string& walletId);
   void needUsedAddresses(const std::string& walletId);
   void needAddrComments(const std::string& walletId, const std::vector<bs::Address>&);

public slots:
   void onSelectionChanged();
   void onDoubleClicked(const QModelIndex& index);

private:
   bs::Address getAddress(const QModelIndex& index) const;

private:
   std::unique_ptr<Ui::SelectAddressDialog>  ui_;
   [[deprecated]] std::vector<std::shared_ptr<bs::sync::Wallet>>  wallets_;
   [[deprecated]] std::shared_ptr<bs::sync::WalletsManager>       walletsMgr_;
   [[deprecated]] const AddressListModel::AddressType             addrType_;
   std::unique_ptr<AddressListModel>         model_;
   bs::Address                   selectedAddr_;
};

#endif // __SELECT_ADDRESS_DIALOG_H__
