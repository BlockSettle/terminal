/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletWarningDialog.h"

#include "ui_WalletWarningDialog.h"

WalletWarningDialog::WalletWarningDialog(QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::WalletWarningDialog())
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &WalletWarningDialog::accept);
}

WalletWarningDialog::~WalletWarningDialog() = default;
