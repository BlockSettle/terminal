
#include "PdfBackupQmlPrinter.h"
#include "UiUtils.h"

#include <QPainter>


//
// QmlPdfBackup
//

QmlPdfBackup::QmlPdfBackup(QQuickItem *parent)
   : QQuickPaintedItem(parent)
   , pdf_(new WalletBackupPdfWriter(walletId_, part1_, part2_,
      QPixmap(QLatin1String(":/FULL_LOGO")),
      UiUtils::getQRCode(part1_ + QLatin1Literal("\n") + part2_)))
{
   connect(this, &QQuickPaintedItem::widthChanged, this, &QmlPdfBackup::onWidthChanged);
}

void QmlPdfBackup::onWidthChanged()
{
   emit preferedHeightChanged();
}

void QmlPdfBackup::paint(QPainter *painter)
{
   int viewportWidth = static_cast<int>(width());
   int viewportHeight = qRound(viewportWidth * kTotalHeightInches / kTotalWidthInches);

   int windowWidth = qRound((kTotalWidthInches - kMarginInches * 2.0) * kResolution);
   int windowHeight = qRound((kTotalHeightInches - kMarginInches * 2.0) * kResolution);

   int viewportMargin = qRound(kMarginInches / kTotalWidthInches * viewportWidth * 0.5);

   painter->setRenderHint(QPainter::SmoothPixmapTransform);

   painter->fillRect(0, 0, viewportWidth, viewportHeight, Qt::white);

   painter->setViewport(viewportMargin, viewportMargin
     , viewportWidth - 2 * viewportMargin
     , viewportHeight - 2 * viewportMargin);

   painter->setWindow(0, 0, windowWidth, windowHeight);

   // The code in draw does not work correctly with other sizes than A4 and 1200 DPI.
   // So we keep logical sizes and use viewport an window instead.
   pdf_->draw(*painter, windowWidth, windowHeight);
}

qreal QmlPdfBackup::preferedHeight() const
{
   return (width() * kTotalHeightInches / kTotalWidthInches);
}

const QString& QmlPdfBackup::walletId() const
{
   return walletId_;
}

void QmlPdfBackup::setWalletId(const QString &id)
{
   walletId_ = id;

   pdf_.reset(new WalletBackupPdfWriter(walletId_, part1_, part2_,
         QPixmap(QLatin1String(":/FULL_LOGO")),
         UiUtils::getQRCode(part1_ + QLatin1Literal("\n") + part2_)));

   update();
}

const QString& QmlPdfBackup::part1() const
{
   return part1_;
}

void QmlPdfBackup::setPart1(const QString &p1)
{
   part1_ = p1;

   pdf_.reset(new WalletBackupPdfWriter(walletId_, part1_, part2_,
         QPixmap(QLatin1String(":/FULL_LOGO")),
         UiUtils::getQRCode(part1_ + QLatin1Literal("\n") + part2_)));

   update();
}

const QString& QmlPdfBackup::part2() const
{
   return part2_;
}

void QmlPdfBackup::setPart2(const QString &p2)
{
   part2_ = p2;

   pdf_.reset(new WalletBackupPdfWriter(walletId_, part1_, part2_,
         QPixmap(QLatin1String(":/FULL_LOGO")),
         UiUtils::getQRCode(part1_ + QLatin1Literal("\n") + part2_)));

   update();
}
