/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include "PDFWriter.h"


PDFWriter::PDFWriter(const QString &templateFN, const QUrl &baseUrl) : printer_(QPrinter::HighResolution)
{
   printer_.setOutputFormat(QPrinter::PdfFormat);
   printer_.setPaperSize(QPrinter::A4);
   doc_.setPageSize(printer_.pageRect().size());

   if (templateFN.isEmpty()) {
      throw std::invalid_argument("improper template filename");
   }
   QFile f(templateFN);
   if (!f.exists()) {
      throw std::invalid_argument(templateFN.toStdString() + " doesn't exist");
   }
   if (!f.open(QIODevice::ReadOnly)) {
      throw std::runtime_error("failed to open " + templateFN.toStdString() + " for reading");
   }
   templateText_ = QString::fromUtf8(f.readAll());

   if (baseUrl.isEmpty()) {
      QFileInfo fi(templateFN);
      doc_.setBaseUrl(QLatin1String("file://") + fi.path());
   }
   else {
      doc_.setBaseUrl(baseUrl);
   }
}

bool PDFWriter::substitute(const QVariantHash &vars)
{
   if (templateText_.isEmpty()) {
      return false;
   }
   substitutedText_ = templateText_;

   for (auto var = vars.begin(); var != vars.end(); ++var) {
      if (var.key().isEmpty()) {
         continue;
      }
      const auto replaceStr = QLatin1String("%") + var.key() + QLatin1String("%");
      substitutedText_.replace(replaceStr, var.value().toString());
   }
   return true;
}

bool PDFWriter::output(const QString &outputFN)
{
   if (substitutedText_.isEmpty()) {
      return false;
   }
   printer_.setOutputFileName(outputFN);
   {
      QPainter painter;
      if (!painter.begin(&printer_)) {
         return false;
      }
      painter.end();
   }
   doc_.setHtml(substitutedText_);
   doc_.print(&printer_);
   substitutedText_.clear();
   return true;
}
