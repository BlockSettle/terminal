/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SELECT_WALLET_DIALOG_H__
#define __SELECT_WALLET_DIALOG_H__

#include <QDialog>
#include <memory>
#include "SignerDefs.h"

namespace Ui {
    class SelectWalletDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}
class WalletsViewModel;
class ApplicationSettings;


class SelectWalletDialog : public QDialog
{
Q_OBJECT

public:
   [[deprecated]] SelectWalletDialog(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::string &selWalletId, QWidget* parent = nullptr);
   SelectWalletDialog(const std::string& selWalletId, QWidget* parent = nullptr);
   ~SelectWalletDialog() override;

   std::string getSelectedWallet() const;

   void onHDWallet(const bs::sync::WalletInfo&);
   void onHDWalletDetails(const bs::sync::HDWalletData&);
   void onWalletBalances(const bs::sync::WalletBalanceData&);

public slots:
   void onSelectionChanged();
   void onDoubleClicked(const QModelIndex& index);

private:
   std::unique_ptr<Ui::SelectWalletDialog> ui_;
   WalletsViewModel  *walletsModel_;
   std::string       selectedWallet_;
};

#endif // __SELECT_WALLET_DIALOG_H__
