/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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
#include "qrc_CommonUI.cpp"

//
// WalletBackupPdfWriter
//

WalletBackupPdfWriter::WalletBackupPdfWriter(const QString &walletId
   , const QString &keyLine1, const QString &keyLine2
   , const QPixmap &qr)
   : walletId_(walletId)
   , keyLine1_(keyLine1)
   , keyLine2_(keyLine2)
   , qr_(qr)
{
}

bool WalletBackupPdfWriter::write(const QString &fileName)
{
   QFile f(fileName);
   bool success = f.open(QIODevice::WriteOnly);

   if (!success)
      return false;

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
   QFont font = p.font();
   font.setPixelSize(175);

   QPixmap logo = QPixmap(QString::fromUtf8("://resources/RPK_template.png"));
   p.drawPixmap(QRect(0, 0, width, height), logo);

   qreal relWidth = width / logo.width();
   qreal relHeight = height / logo.height();

   QFont italic = font;
   italic.setItalic(true);

   QFont bold = font;
   bold.setBold(true);

   QStaticText wIdDesc(QLatin1String("Wallet ID"));
   QStaticText keyLine1Desc(QLatin1String("Line 1"));
   QStaticText keyLine2Desc(QLatin1String("Line 2"));

   QStaticText wId(walletId_);
   QStaticText keyLine1(keyLine1_);
   QStaticText keyLine2(keyLine2_);

   wIdDesc.prepare(QTransform(), italic);
   keyLine1Desc.prepare(QTransform(), italic);
   keyLine2Desc.prepare(QTransform(), italic);

   wId.prepare(QTransform(), bold);
   keyLine1.prepare(QTransform(), bold);
   keyLine2.prepare(QTransform(), bold);

   p.setFont(bold);

   // Magic numbers is pixel positions of given controls on RPK_template.png
   p.drawStaticText(QPointF(500 * relWidth, 1530 * relHeight), wId);
   p.drawStaticText(QPointF(110 * relWidth, 1940 * relHeight), keyLine1);
   p.drawStaticText(QPointF(110 * relWidth, 2100 * relHeight), keyLine2);

   p.drawPixmap(QRectF(1224 * relWidth, 1611 * relHeight, 569 * relWidth, 569 * relHeight),
      qr_, qr_.rect());
}
