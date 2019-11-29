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

//
// WalletBackupPdfWriter
//

namespace {

const int kResolution = 1200;

const qreal kTotalWidthInches = 8.27;
const qreal kTotalHeightInches = 11.69;
const qreal kMarginInches = 1.0;

}

WalletBackupPdfWriter::WalletBackupPdfWriter(const QString &walletId
   , const QString &keyLine1, const QString &keyLine2
   , const QPixmap &logo, const QPixmap &qr)
   : walletId_(walletId)
   , keyLine1_(keyLine1)
   , keyLine2_(keyLine2)
   , logo_(logo)
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
   const qreal m = 400.0;

   qreal y = 0.0;

   p.drawPixmap(logo_.rect(), logo_);

   y += logo_.height() + m;

   {
      QFont f = font;
      f.setPixelSize(300);
      p.setFont(f);

      QRectF r;
      p.drawText(QRectF(0.0, y, width, height - y), 0, QLatin1String("Root Private Key"), &r);
      y += r.height() + m;
   }

   {
      p.setFont(font);

      QStaticText text(QLatin1String(
         "<span style=\"color:#EC0A35\"><b>WARNING!</b></span> "
         "Anyone who has access to this unencrypted paper backup "
         "of your Root Private Key will be able to access all bitcoins in this"
         " wallet! Please keep this paper backup in a safe place."));
      QTextOption opt;
      opt.setWrapMode(QTextOption::WordWrap);
      text.setTextOption(opt);
      text.setTextWidth(width);

      p.drawStaticText(QPointF(0.0, y), text);

      y += text.size().height() + m;
   }

   auto qrY = y;

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

   const qreal wDescWidth = wIdDesc.size().width();
   const qreal wWidth = wId.size().width();
   const qreal keyDescWidth = qMax(keyLine1Desc.size().width(), keyLine2Desc.size().width());
   const qreal keyWidth = qMax(keyLine1.size().width(), keyLine2.size().width());
   const qreal space = 100.0;

   p.setFont(italic);
   p.drawStaticText(QPointF(0.0, y), wIdDesc);
   y += (wIdDesc.size().height() + m) * 2;

   p.setFont(bold);

   QStaticText keyDesc(QLatin1String("Root Key:"));
   p.drawStaticText(QPointF(0.0, y), keyDesc);
   y += keyDesc.size().height() + m;
   const qreal key1Y = y;

   p.setFont(italic);
   p.drawStaticText(QPointF(0.0, y), keyLine1Desc);
   y += keyLine1Desc.size().height() + m;
   const qreal key2Y = y;
   p.drawStaticText(QPointF(0.0, y), keyLine2Desc);
   y += keyLine2Desc.size().height() + m;
   const qreal ty = y;

   p.save();
   p.setBrush(QColor(0xF3, 0xF3, 0xF3));
   p.setPen(Qt::NoPen);

   const qreal offset = 50.0;

   p.drawRect(wDescWidth + space - offset, qrY - m / 2.0 + offset,
              wWidth + offset * 2, wId.size().height() + m - offset * 2);

   p.drawRect(keyDescWidth + space - offset, key1Y - m / 2.0 + offset,
              keyWidth + offset * 2, keyLine1.size().height() + m - offset * 2);
   p.drawRect(keyDescWidth + space - offset, key2Y - m / 2.0 + offset,
              keyWidth + offset * 2, keyLine1.size().height() + m - offset * 2);
   p.restore();

   p.setFont(bold);
   p.drawStaticText(QPointF(wDescWidth + space, qrY), wId);
   p.drawStaticText(QPointF(keyDescWidth + space, key1Y), keyLine1);
   p.drawStaticText(QPointF(keyDescWidth + space, key2Y), keyLine2);

   qrY = qrY - m / 2.0 + offset;
   const qreal qrX = qMax((wDescWidth + wWidth + space + offset * 2),
                          (keyDescWidth + keyWidth + space + offset * 2));

   qreal qrWidth = qMin(width - qrX, key1Y - qrY - m / 2.0 + offset);
   qrWidth = qMin(qrWidth, 2048.0);

   p.drawPixmap(QRectF(width - qrWidth, qrY, qrWidth, qrWidth),
                qr_, qr_.rect());

   QStaticText bottom(QLatin1String(
      "<p style=\"margin-bottom:200px;\">The Root Private Key can be used to recover your wallet if you forgot your password or "
      "suffer hardware failure and lose your wallet files.</p></br>"
      "<p style=\"margin-bottom:200px;\">BlockSettle uses Hierarchical Deterministic (HD) wallets to make it easy to derive many "
      "public keys from a single Root Private Key.</p></br>"
      "<p style=\"margin-bottom:200px;\">HD wallets contain keys derived in a tree structure, such that a Root Key can derive a "
      "sequence of branch keys, which each can derive their own branches of keys, and so on.</p></br>"
      "<p style=\"margin-bottom:200px;\">BlockSettle makes extensive use of this technology to sort Private Market Share "
      "tokens and Authentication Addresses from general addresses used for sending and receiving "
      "bitcoins.</p></br>"
      "<p>The Root Private Key will restore all addresses ever generated by this wallet.</p>"));
   QTextOption opt;
   opt.setWrapMode(QTextOption::WordWrap);
   bottom.setTextOption(opt);
   bottom.setTextWidth(width);

   p.setFont(font);
   p.drawStaticText(QPointF(0.0, ty), bottom);

   y = ty + bottom.size().height() + m * 2;

   QStaticText footer(QString::fromUtf8(
      "<p style=\"color:#888;\" >Ostergatan 21, 211 25 Malmö, Sweden | support@blocksettle.com</p>"));
   footer.setTextOption(opt);
   footer.setTextWidth(width);

   QFont f = font;
   f.setPixelSize(125);
   p.setFont(f);

   p.drawStaticText(QPointF(0.0, y), footer);
}
