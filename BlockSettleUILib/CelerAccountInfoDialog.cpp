/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerAccountInfoDialog.h"
#include "ui_CelerAccountInfoDialog.h"

#include "CelerClient.h"

CelerAccountInfoDialog::CelerAccountInfoDialog(std::shared_ptr<BaseCelerClient> celerConnection, QWidget* parent)
 : QDialog(parent)
 , ui_(new Ui::CelerAccountInfoDialog())
{
   ui_->setupUi(this);
   ui_->labelEmailAddress->setText(QString::fromStdString(celerConnection->email()));
   ui_->labelUserType->setText(celerConnection->userType());
   connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &CelerAccountInfoDialog::reject);
}

CelerAccountInfoDialog::~CelerAccountInfoDialog() = default;
