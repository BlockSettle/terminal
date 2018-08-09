
#include "PaperBackupWriter.h"

#include <QPdfWriter>
#include <QPainter>
#include <QStaticText>


//
// WalletBackupPdfWriter
//

WalletBackupPdfWriter::WalletBackupPdfWriter(const QString &walletName,
   const QString &walletId, const QString &keyLine1, const QString &keyLine2,
   const QPixmap &logo, const QPixmap &qr)
   : walletName_(walletName)
   , walletId_(walletId)
   , keyLine1_(keyLine1)
   , keyLine2_(keyLine2)
   , logo_(logo)
   , qr_(qr)
{
}

bool WalletBackupPdfWriter::write(const QString &fileName)
{
   const int resolution = 1200;
   const qreal margin = 1.0;
   const qreal width = (8.26 - margin * 2.0) * resolution;
   const qreal height = (11.69 - margin * 2.0) * resolution;

   QPdfWriter pdf(fileName);
   pdf.setPageSize(QPagedPaintDevice::A4);
   pdf.setResolution(resolution);

   QPageLayout layout = pdf.pageLayout();
   layout.setUnits(QPageLayout::Inch);
   layout.setMargins(QMarginsF(margin, margin, margin, margin));
   pdf.setPageLayout(layout);

   QPainter p(&pdf);
   draw(p, width, height);
   p.end();

   return true;
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
         "Anyone who has access to this Unencrypted Paper Backup "
         "of your Root Private Key will be able to access all bitcoins in this"
         " wallet! Please keep this Paper Backup in a safe place."));
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

   QStaticText wNameDesc(QLatin1String("Wallet Name"));
   QStaticText wIdDesc(QLatin1String("Wallet ID"));
   QStaticText keyLine1Desc(QLatin1String("Line 1"));
   QStaticText keyLine2Desc(QLatin1String("Line 2"));

   QStaticText wName(walletName_);
   QStaticText wId(walletId_);
   QStaticText keyLine1(keyLine1_);
   QStaticText keyLine2(keyLine2_);

   wNameDesc.prepare(QTransform(), italic);
   wIdDesc.prepare(QTransform(), italic);
   keyLine1Desc.prepare(QTransform(), italic);
   keyLine2Desc.prepare(QTransform(), italic);

   wName.prepare(QTransform(), bold);
   wId.prepare(QTransform(), bold);
   keyLine1.prepare(QTransform(), bold);
   keyLine2.prepare(QTransform(), bold);

   const qreal wDescWidth = qMax(wNameDesc.size().width(), wIdDesc.size().width());
   const qreal wWidth = qMax(wName.size().width(), wId.size().width());
   const qreal keyDescWidth = qMax(keyLine1Desc.size().width(), keyLine2Desc.size().width());
   const qreal keyWidth = qMax(keyLine1.size().width(), keyLine2.size().width());
   const qreal space = 100.0;

   p.setFont(italic);
   p.drawStaticText(QPointF(0.0, y), wNameDesc);
   y += wNameDesc.size().height() + m;
   const qreal wIdY = y;
   p.drawStaticText(QPointF(0.0, y), wIdDesc);
   y += wIdDesc.size().height() + m;

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
              wWidth + offset * 2, wName.size().height() + m - offset * 2);
   p.drawRect(wDescWidth + space - offset, wIdY - m / 2.0 + offset,
              wWidth + offset * 2, wName.size().height() + m - offset * 2);

   p.drawRect(keyDescWidth + space - offset, key1Y - m / 2.0 + offset,
              keyWidth + offset * 2, keyLine1.size().height() + m - offset * 2);
   p.drawRect(keyDescWidth + space - offset, key2Y - m / 2.0 + offset,
              keyWidth + offset * 2, keyLine1.size().height() + m - offset * 2);
   p.restore();

   p.setFont(bold);
   p.drawStaticText(QPointF(wDescWidth + space, qrY), wName);
   p.drawStaticText(QPointF(wDescWidth + space, wIdY), wId);
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
      "<p style=\"margin-bottom:200px;\">BlockSettle uses Hierarchical Deterministic (HD) Wallets to make it easy to derive many "
      "Public keys from a single Root Private Key.</p></br>"
      "<p style=\"margin-bottom:200px;\">HD Wallets contain keys derived in a tree structure, such that a Root Key can derive a "
      "sequence of branch keys, which each can derive their own branches of keys, and so on.</p></br>"
      "<p style=\"margin-bottom:200px;\">BlockSettle makes extensive use of this technology to sort Private Market Share "
      "tokens and Authentication Addresses from general addresses used for sending and receiving "
      "bitcoin.</p></br>"
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
