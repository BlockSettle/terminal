
#include "NewWalletSeed.h"
#include "BtcDefinitions.h"
#include "MetaData.h"
#include "EasyCoDec.h"
#include "UiUtils.h"

#include <QPrinter>
#include <QPrintDialog>
#include <QDir>
#include <QStandardPaths>
#include <QFileDialog>


//
// NewWalletSeed
//

NewWalletSeed::NewWalletSeed(std::shared_ptr<SignerSettings> settings, QObject *parent)
   : QObject(parent)
   , settings_(settings)
{
}

const QString& NewWalletSeed::walletId() const
{
   return walletId_;
}

const QString& NewWalletSeed::part1() const
{
   return part1_;
}

const QString& NewWalletSeed::part2() const
{
   return part2_;
}

void NewWalletSeed::generate()
{
   NetworkType netType = (settings_->testNet() ? NetworkType::TestNet : NetworkType::MainNet);

   bs::wallet::Seed walletSeed(netType, SecureBinaryData().GenerateRandom(32));

   EasyCoDec::Data easyData = walletSeed.toEasyCodeChecksum();

   walletId_ = QString::fromStdString(bs::hd::Node(walletSeed).getId());
   part1_ = QString::fromStdString(easyData.part1);
   part2_ = QString::fromStdString(easyData.part2);

   pdfWriter_.reset(new WalletBackupPdfWriter(walletId_, part1_, part2_,
      QPixmap(QLatin1String(":/FULL_LOGO")),
      UiUtils::getQRCode(part1_ + QLatin1Literal("\n") + part2_)));
}

void NewWalletSeed::print()
{
   QPrinter printer(QPrinter::PrinterResolution);

   printer.setOutputFormat(QPrinter::NativeFormat);

   // Check if printers installed because QPrintDialog won't work otherwise.
   // See https://bugreports.qt.io/browse/QTBUG-36112
   // Happens on macOS with Qt 5.11.1
   if (printer.outputFormat() != QPrinter::NativeFormat) {
      emit unableToPrint();
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

   pdfWriter_->print(&printer);
}

void NewWalletSeed::save()
{
   QDir documentsDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
   QString filePath = documentsDir.filePath(QString::fromLatin1("backup_wallet_%1.pdf").arg(walletId_));

   QFileDialog dlg;
   dlg.setFileMode(QFileDialog::AnyFile);
   filePath = dlg.getSaveFileName(nullptr,
      tr("Select file for backup"), filePath, QLatin1String("*.pdf"));

   if (filePath.isEmpty()) {
      return;
   }

   bool result = pdfWriter_->write(filePath);

   if (!result) {
      emit failedToSave(filePath);
   }
}
