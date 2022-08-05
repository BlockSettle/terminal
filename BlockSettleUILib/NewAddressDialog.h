/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __NEW_ADDRESS_DIALOG_H__
#define __NEW_ADDRESS_DIALOG_H__

#include <QDialog>
#include <memory>
#include "Address.h"
#include "Wallets/SignerDefs.h"

namespace Ui {
   class NewAddressDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
   }
}
class QPushButton;

class NewAddressDialog : public QDialog
{
Q_OBJECT

public:
   NewAddressDialog(const bs::sync::WalletInfo &, QWidget* parent = nullptr);
   ~NewAddressDialog() override;

   void onAddresses(const std::string& walletId, const std::vector<bs::sync::Address>&);

protected:
   void showEvent(QShowEvent* event) override;

private slots:
   void copyToClipboard();
   void onClose();

private:
   void displayAddress();
   void UpdateSizeToAddress();

private:
   std::unique_ptr<Ui::NewAddressDialog>  ui_;
   QPushButton* copyButton_{ nullptr };
   QPushButton* closeButton_{ nullptr };
   std::string    walletId_;
   bs::Address    address_;
};

#endif // __NEW_ADDRESS_DIALOG_H__
