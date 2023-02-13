/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ApiKeyEntryDialog.h"

#include "ui_ApiKeyEntryDialog.h"

ApiKeyEntryDialog::ApiKeyEntryDialog(QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::ApiKeyEntryDialog())
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &QDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &QDialog::reject);
}

QString ApiKeyEntryDialog::getApiKey(QWidget *parent)
{
   ApiKeyEntryDialog dlg(parent);
   int rc = dlg.exec();
   if (rc == QDialog::Rejected) {
      return {};
   }
   return dlg.ui_->plainTextEditApiKey->toPlainText();
}

ApiKeyEntryDialog::~ApiKeyEntryDialog() = default;
