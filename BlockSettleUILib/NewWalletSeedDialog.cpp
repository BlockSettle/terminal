#include "NewWalletSeedDialog.h"

#include <QDir>
#include <QFileDialog>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QStandardPaths>
#include <QValidator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QEvent>

#include "BSMessageBox.h"
#include "PaperBackupWriter.h"
#include "UiUtils.h"
#include "ui_NewWalletSeedDialog.h"
#include "make_unique.h"
#include "EasyEncValidator.h"

namespace {
   const double kMarginScale = 0.5;
}

NewWalletSeedDialog::NewWalletSeedDialog(const QString& walletId
   , const QString &keyLine1, const QString &keyLine2, QWidget *parent) :
   QDialog(parent)
   , ui_(new Ui::NewWalletSeedDialog)
   , walletId_(walletId)
   , keyLine1_(keyLine1)
   , keyLine2_(keyLine2)
{
   ui_->setupUi(this);

   pdfWriter_.reset(new WalletBackupPdfWriter(walletId, keyLine1, keyLine2
      , QPixmap(QLatin1String(":/resources/logo_print-250px-300ppi.png"))
      , UiUtils::getQRCode(keyLine1 + QLatin1Literal("\n") + keyLine2)));

   ui_->scrollArea->viewport()->installEventFilter(this);

   connect(ui_->pushButtonSave, &QPushButton::clicked, this, &NewWalletSeedDialog::onSaveClicked);
   connect(ui_->pushButtonPrint, &QPushButton::clicked, this, &NewWalletSeedDialog::onPrintClicked);
   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &NewWalletSeedDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &NewWalletSeedDialog::reject);
}

NewWalletSeedDialog::~NewWalletSeedDialog() = default;

bool NewWalletSeedDialog::eventFilter(QObject *obj, QEvent *event)
{
   if (obj == ui_->scrollArea->viewport() && event->type() == QEvent::Resize) {
      QResizeEvent *e = static_cast<QResizeEvent*>(event);

      const int w = e->size().width();

      const auto pdfPreview = pdfWriter_->getPreview(w, kMarginScale);

      ui_->labelPreview->setPixmap(pdfPreview);

      return false;
   } else {
      return QDialog::eventFilter(obj, event);
   }
}

void NewWalletSeedDialog::onSaveClicked()
{
   QDir documentsDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
   QString filePath = documentsDir.filePath(QString::fromLatin1("backup_wallet_%1.pdf").arg(walletId_));

   QFileDialog dlg;
   dlg.setFileMode(QFileDialog::AnyFile);
   filePath = dlg.getSaveFileName(this, tr("Select file for backup"), filePath, QLatin1String("*.pdf"));

   if (filePath.isEmpty()) {
      return;
   }

   bool result = pdfWriter_->write(filePath);

   if (!result) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Failed to save backup file")
         , tr("Unable to open file %1 for writing").arg(filePath));
      messageBox.exec();
      return;
   }
}

void NewWalletSeedDialog::reject()
{
   bool result = MessageBoxWalletCreateAbort(this).exec();
   if (result) {
      QDialog::reject();
   }
}

void NewWalletSeedDialog::onPrintClicked()
{
   QPrinter printer(QPrinter::PrinterResolution);

   printer.setOutputFormat(QPrinter::NativeFormat);

   // Check if printers installed because QPrintDialog won't work otherwise.
   // See https://bugreports.qt.io/browse/QTBUG-36112
   // Happens on macOS with Qt 5.11.11
   if (printer.outputFormat() != QPrinter::NativeFormat) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Print Error")
         , tr("Please make sure that you have a printer connected."), this);
      messageBox.exec();
      return;
   }

   QPrintDialog dialog(&printer, this);
   dialog.setWindowTitle(tr("Print Wallet Seed"));
   dialog.setPrintRange(QAbstractPrintDialog::CurrentPage);
   dialog.setMinMax(1, 1);

   int result = dialog.exec();
   if (result != QDialog::Accepted) {
      return;
   }

   pdfWriter_->print(&printer);
}
