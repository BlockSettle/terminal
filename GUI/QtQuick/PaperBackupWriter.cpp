/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "PaperBackupWriter.h"

#include <QFile>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QStaticText>

WalletBackupPdfWriter::WalletBackupPdfWriter(const QString &walletId
   , const QStringList& seed
   , const QPixmap &qr)
   : walletId_(walletId)
   , seed_(seed)
   , qr_(qr)
{
}

bool WalletBackupPdfWriter::write(const QString &fileName)
{
   QFile f(fileName);
   bool success = f.open(QIODevice::WriteOnly);

   if (!success) {
       return false;
   }

   QPdfWriter pdf(fileName);

   pdf.setPageSize(QPagedPaintDevice::A4);
   pdf.setResolution(kResolution);

   qreal width = (kTotalWidthInches - kMarginInches * 2.0) * kResolution;
   qreal height = (kTotalHeightInches - kMarginInches * 2.0) * kResolution;

   QPageLayout layout = pdf.pageLayout();
   layout.setUnits(QPageLayout::Inch);
   layout.setMargins(QMarginsF(kMarginInches, kMarginInches, kMarginInches, kMarginInches));
   pdf.setPageLayout(layout);

   QPainter p(&pdf);
   draw(p, width, height);
   p.end();

   f.close();
   success = (f.error() == QFileDevice::NoError && f.size() > 0);

   return success;
}

QPixmap WalletBackupPdfWriter::getPreview(int width, double marginScale)
{
   int viewportWidth = width;
   int viewportHeight = qRound(viewportWidth * kTotalHeightInches / kTotalWidthInches);

   int windowWidth = qRound((kTotalWidthInches - kMarginInches * 2.0) * kResolution);
   int windowHeight = qRound((kTotalHeightInches - kMarginInches * 2.0) * kResolution);

   int viewportMargin = qRound(kMarginInches / kTotalWidthInches * viewportWidth * marginScale);

   QPixmap image(viewportWidth, viewportHeight);
   image.fill(Qt::white);

   QPainter painter(&image);

   painter.setRenderHint(QPainter::SmoothPixmapTransform);

   painter.setViewport(viewportMargin, viewportMargin
     , viewportWidth - 2 * viewportMargin
     , viewportHeight - 2 * viewportMargin);

   painter.setWindow(0, 0, windowWidth, windowHeight);

   // The code in draw does not work correctly with other sizes than A4 and 1200 DPI.
   // So we keep logical sizes and use viewport an window instead.
   draw(painter, windowWidth, windowHeight);

   painter.end();

   return image;
}

void WalletBackupPdfWriter::print(QPrinter *printer)
{
   int printerResolution = printer->resolution();

   int viewportWidth = qRound((kTotalWidthInches - kMarginInches * 2.0) * printerResolution);
   int viewportHeight = qRound((kTotalHeightInches - kMarginInches * 2.0) * printerResolution);

   printer->setPageMargins(QMarginsF(kMarginInches, kMarginInches, kMarginInches, kMarginInches), QPageLayout::Inch);

   int windowWidth = qRound((kTotalWidthInches - kMarginInches * 2.0) * kResolution);
   int windowHeight = qRound((kTotalHeightInches - kMarginInches * 2.0) * kResolution);

   QPainter painter(printer);

   painter.setRenderHint(QPainter::SmoothPixmapTransform);
   painter.setViewport(0, 0, viewportWidth, viewportHeight);
   painter.setWindow(0, 0, windowWidth, windowHeight);

   // The code in draw does not work correctly with sizes other than A4/Letter and 1200 DPI.
   // So we keep logical sizes and use viewport an window instead.
   draw(painter, windowWidth, windowHeight);
}

void WalletBackupPdfWriter::draw(QPainter &p, qreal width, qreal height)
{
   const auto longSeed = seed_.length() > 12;

   QFont font = p.font();
   font.setPixelSize(150);
   font.setBold(true);

   QPixmap logo = QPixmap(QString::fromUtf8(longSeed ? ":/images/RPK24.png" : ":/images/RPK12.png"));
   p.drawPixmap(QRect(0, 0, width, height), logo);

   qreal relWidth = width / logo.width();
   qreal relHeight = height / logo.height();


   p.setFont(font);

   // Magic numbers is pixel positions of given controls on RPK_template.png
   p.drawText(QPointF(140 * relWidth, 2020 * relHeight), walletId_);

   const auto topLeftX = 240;
   const auto topLeftY = 2415;
   const auto deltaX = 480;
   const auto deltaY = 93;
   for (auto i = 0; i < 3; ++i) {
      for (auto j = 0; j < seed_.size() / 3; ++j) {
         p.drawText(QPointF((topLeftX + i * deltaX) * relWidth, (topLeftY + j * deltaY) * relHeight), seed_.at(i * 3 + j));
      }
   }

   p.drawPixmap(QRectF(1775 * relWidth, 1970 * relHeight, 790 * relWidth, 790 * relHeight),
      qr_, qr_.rect());
}
