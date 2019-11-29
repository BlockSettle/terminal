/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __PDF_WRITER_H__
#define __PDF_WRITER_H__

#include <QPrinter>
#include <QTextDocument>
#include <QString>
#include <QUrl>
#include <QVariant>


class PDFWriter
{
public:
   PDFWriter(const QString &templateFN, const QUrl &baseUrl = {});

   bool substitute(const QVariantHash &vars);
   bool output(const QString &outputFN);

private:
   QPrinter       printer_;
   QTextDocument  doc_;
   QString        templateText_;
   QString        substitutedText_;
};

#endif // __PDF_WRITER_H__
