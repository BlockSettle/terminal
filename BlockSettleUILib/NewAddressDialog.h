/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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

namespace Ui {
   class NewAddressDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
   }
}
class SignContainer;


class NewAddressDialog : public QDialog
{
Q_OBJECT

public:
   NewAddressDialog(const std::shared_ptr<bs::sync::Wallet>& wallet
      , const std::shared_ptr<SignContainer> &, QWidget* parent = nullptr);
   ~NewAddressDialog() override;

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
   std::shared_ptr<bs::sync::Wallet>      wallet_;
   bs::Address    address_;
};

#endif // __NEW_ADDRESS_DIALOG_H__
