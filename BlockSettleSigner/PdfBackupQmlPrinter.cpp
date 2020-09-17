/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "PdfBackupQmlPrinter.h"
#include "UiUtils.h"

#include <QPainter>
#include <QPrinter>
#include <QPrintDialog>
#include <QDir>
#include <QStandardPaths>
#include <QFileDialog>
#include <QDesktopServices>

#include "PaperBackupWriter.h"

//
// QmlPdfBackup
//


QmlPdfBackup::QmlPdfBackup(QQuickItem *parent)
   : QQuickPaintedItem(parent)
   , seed_(new bs::wallet::QSeed(this))
   , pdf_(new WalletBackupPdfWriter(QString(), QString(), QString(),
      UiUtils::getQRCode(QString())))
{
   connect(this, &QQuickPaintedItem::widthChanged, this, &QmlPdfBackup::onWidthChanged);
   connect(this, &QmlPdfBackup::seedChanged, this, &QmlPdfBackup::onSeedChanged);
}

void QmlPdfBackup::onWidthChanged()
{
   emit preferredHeightForWidthChanged();
}

void QmlPdfBackup::onSeedChanged()
{
   if (!seed_) {
      return;
   }
   pdf_.reset(new WalletBackupPdfWriter(seed_->walletId(), seed_->part1(), seed_->part2(),
      UiUtils::getQRCode(seed_->part1() + QLatin1Literal("\n") + seed_->part2())));

   update();
}

bs::wallet::QSeed *QmlPdfBackup::seed() const
{
   return seed_;
}

void QmlPdfBackup::setSeed(bs::wallet::QSeed *seed)
{
   seed_ = seed;
   emit seedChanged();
}

void QmlPdfBackup::paint(QPainter *painter)
{
   int viewportWidth = static_cast<int>(width());
   int viewportHeight = qRound(viewportWidth * WalletBackupPdfWriter::kTotalHeightInches / WalletBackupPdfWriter::kTotalWidthInches);

   int windowWidth = qRound((WalletBackupPdfWriter::kTotalWidthInches - WalletBackupPdfWriter::kMarginInches * 2.0)
                            * WalletBackupPdfWriter::kResolution);
   int windowHeight = qRound((WalletBackupPdfWriter::kTotalHeightInches - WalletBackupPdfWriter::kMarginInches * 2.0)
                            * WalletBackupPdfWriter::kResolution);

   int viewportMargin = qRound(WalletBackupPdfWriter::kMarginInches / WalletBackupPdfWriter::kTotalWidthInches * viewportWidth * 0.5);

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

qreal QmlPdfBackup::preferredHeightForWidth() const
{
   return (width() * WalletBackupPdfWriter::kTotalHeightInches / WalletBackupPdfWriter::kTotalWidthInches);
}

void QmlPdfBackup::componentComplete()
{
   QQuickItem::componentComplete();
   emit seedChanged();
}

void QmlPdfBackup::save()
{
   QDir documentsDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
   QString netType = (seed_->networkType() == bs::wallet::QSeed::MainNet) ? QStringLiteral("mainnet") : QStringLiteral("testnet");
   QString filePath = documentsDir.filePath(QStringLiteral("BlockSettle_%1_%2.pdf").arg(netType).arg(seed_->walletId()));

   QFileDialog dlg;
   dlg.setFileMode(QFileDialog::AnyFile);
   filePath = dlg.getSaveFileName(nullptr,
      tr("Select file for backup"), filePath, tr("PDF Files *.pdf"));

   if (filePath.isEmpty()) {
      return;
   }

   bool result = pdf_->write(filePath);

   if (!result) {
      emit saveFailed(filePath);
   }
   else {
      QDesktopServices::openUrl(filePath);
      emit saveSucceed(filePath);
   }
}

void QmlPdfBackup::print()
{
   QPrinter printer(QPrinter::PrinterResolution);

   printer.setOutputFormat(QPrinter::NativeFormat);

   // Check if printers installed because QPrintDialog won't work otherwise.
   // See https://bugreports.qt.io/browse/QTBUG-36112
   // Happens on macOS with Qt 5.11.1
   if (printer.outputFormat() != QPrinter::NativeFormat) {
      emit printFailed();
      return;
   }

   // QPrintDialog doesn't want to work in QML,
   // use QPrinterInfo to get all available printers
   // and create Ui so that user can select the printer to use
   QPrintDialog dialog(&printer);
   dialog.setWindowTitle(tr("Print Wallet Seed"));
   dialog.setPrintRange(QAbstractPrintDialog::CurrentPage);
   dialog.setMinMax(1, 1);

   int result = dialog.exec();
   if (result != QDialog::Accepted) {
      return;
   }

   pdf_->print(&printer);
}

