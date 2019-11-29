/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SupportDialog.h"

#include <QDesktopServices>

SupportDialog::SupportDialog(QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::SupportDialog)

{
   ui_->setupUi(this);
}

SupportDialog::~SupportDialog() = default;

