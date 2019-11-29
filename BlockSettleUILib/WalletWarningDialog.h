/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __WALLET_WARNING_DIALOG_H__
#define __WALLET_WARNING_DIALOG_H__

#include <QDialog>

#include <memory>

namespace Ui
{
   class WalletWarningDialog;
};

class WalletWarningDialog : public QDialog
{
Q_OBJECT

public:
   WalletWarningDialog(QWidget* parent = nullptr);
   ~WalletWarningDialog() override;

private:
   std::unique_ptr<Ui::WalletWarningDialog> ui_;
};


#endif // __WALLET_WARNING_DIALOG_H__
