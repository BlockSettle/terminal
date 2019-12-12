/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MDAgreementDialog.h"
#include "ui_MDAgreementDialog.h"

#include <QFile>
#include <QPushButton>

MDAgreementDialog::MDAgreementDialog(QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::MDAgreementDialog())
{
   ui_->setupUi(this);

   QFile file;
   file.setFileName(QLatin1String("://resources/md_license.html"));
   file.open(QIODevice::ReadOnly);

   QString licenseText = QString::fromUtf8(file.readAll());

   ui_->textBrowser_agreement->setHtml(licenseText);

   connect(ui_->pushButton_Continue, &QPushButton::clicked, this, &MDAgreementDialog::OnContinuePressed);
}

void MDAgreementDialog::OnContinuePressed()
{
   if (ui_->radioButton_accept->isChecked()) {
      accept();
   } else {
      reject();
   }
}