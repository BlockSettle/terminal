/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SupportDialog.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

SupportDialog::SupportDialog(QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::SupportDialog)

{
   ui_->setupUi(this);

   connect(ui_->tradingHelpLabel, &QLabel::linkActivated, this, &SupportDialog::onGuideLinkActivated);
   connect(ui_->walletHelpLabel, &QLabel::linkActivated, this, &SupportDialog::onGuideLinkActivated);
}

SupportDialog::~SupportDialog() = default;

void SupportDialog::onGuideLinkActivated(const QString &pdfFileName)
{
   const QString filePath = qApp->applicationDirPath().append(QString::fromLatin1("/%1").arg(pdfFileName));
   if (QFileInfo::exists(filePath)) {
      QDir(qApp->applicationDirPath()).remove(pdfFileName);
   }

   QFile guideFile(QString::fromLatin1("://resources/%1").arg(pdfFileName));
   guideFile.copy(filePath);

   QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

