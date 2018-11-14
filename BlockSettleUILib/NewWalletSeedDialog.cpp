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
   , easyCodec_(std::make_shared<EasyCoDec>())
{
   ui_->setupUi(this);

   pdfWriter_.reset(new WalletBackupPdfWriter(walletId, keyLine1, keyLine2
      , QPixmap(QLatin1String(":/resources/logo_print-250px-300ppi.png"))
      , UiUtils::getQRCode(keyLine1 + QLatin1Literal("\n") + keyLine2)));

   ui_->scrollArea->viewport()->installEventFilter(this);

   connect(ui_->pushButtonSave, &QPushButton::clicked, this, &NewWalletSeedDialog::onSaveClicked);
   connect(ui_->pushButtonPrint, &QPushButton::clicked, this, &NewWalletSeedDialog::onPrintClicked);
   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &NewWalletSeedDialog::onContinueClicked);
   connect(ui_->pushButtonBack, &QPushButton::clicked, this, &NewWalletSeedDialog::onBackClicked);
   connect(ui_->lineEditLine1, &QLineEdit::textChanged, this, &NewWalletSeedDialog::onKeyChanged);
   connect(ui_->lineEditLine2, &QLineEdit::textChanged, this, &NewWalletSeedDialog::onKeyChanged);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &NewWalletSeedDialog::reject);

   validator_ = make_unique<EasyEncValidator>(easyCodec_, nullptr, 9, true);
   ui_->lineEditLine1->setValidator(validator_.get());
   ui_->lineEditLine2->setValidator(validator_.get());

   setCurrentPage(Pages::PrintPreview);
   updateState();
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

void NewWalletSeedDialog::setCurrentPage(Pages page)
{
   currentPage_ = page;
   ui_->stackedWidget->setCurrentIndex(int(currentPage_));

   ui_->pushButtonSave->setVisible(page == Pages::PrintPreview);
   ui_->pushButtonPrint->setVisible(page == Pages::PrintPreview);
   ui_->pushButtonBack->setVisible(page == Pages::Confirm);

   updateState();

   // Hide to allow adjust to smaller size
   ui_->labelPreview->setVisible(page == Pages::PrintPreview);

   if (page == Pages::PrintPreview) {
      if (sizePreview_.isValid()) {
         resize(sizePreview_);
      }
   }
   else {
      sizePreview_ = size();
      adjustSize();
   }
}

void NewWalletSeedDialog::updateState()
{
   if (currentPage_ == Pages::PrintPreview) {
      ui_->pushButtonContinue->setEnabled(true);
   } else {
      ui_->pushButtonContinue->setEnabled(keysAreCorrect_);
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

   updateState();
}

void NewWalletSeedDialog::reject()
{
   bool result = abortWalletCreationQuestionDialog(this);
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

   updateState();
}

void NewWalletSeedDialog::onBackClicked()
{
   setCurrentPage(Pages::PrintPreview);
}

void NewWalletSeedDialog::onContinueClicked()
{
   if (currentPage_ == Pages::PrintPreview) {
      setCurrentPage(Pages::Confirm);
   } else {
      validateKeys();
   }
}

void NewWalletSeedDialog::validateKeys()
{
   if (!keysAreCorrect_) {
      BSMessageBox messageBox(BSMessageBox::critical, tr("Check failed!")
         , tr("Input values do not match with the original keys. Please make sure the input lines are correct."));
      messageBox.exec();
      return;
   }

   accept();
}

void NewWalletSeedDialog::onKeyChanged(const QString &)
{
   QString inputLine1 = ui_->lineEditLine1->text().trimmed();
   QString inputLine2 = ui_->lineEditLine2->text().trimmed();
   QString keyLine1 = keyLine1_;
   QString keyLine2 = keyLine2_;

   // Remove all spaces just in case.
   inputLine1.remove(QChar::Space);
   inputLine2.remove(QChar::Space);
   keyLine1.remove(QChar::Space);
   keyLine2.remove(QChar::Space);

   if (inputLine1 != keyLine1 || inputLine2 != keyLine2) {
      keysAreCorrect_ = false;
   } else {
      keysAreCorrect_ = true;
   }

   if (inputLine1 != keyLine1) {
      UiUtils::setWrongState(ui_->lineEditLine1, true);
   } else {
      UiUtils::setWrongState(ui_->lineEditLine1, false);
   }

   if (inputLine2 != keyLine2) {
      UiUtils::setWrongState(ui_->lineEditLine2, true);
   } else {
      UiUtils::setWrongState(ui_->lineEditLine2, false);
   }

   updateState();
}

bool abortWalletCreationQuestionDialog(QWidget* parent)
{
   BSMessageBox messageBox(BSMessageBox::question, QObject::tr("Warning"), QObject::tr("Abort Wallet Creation?")
      , QObject::tr("The Wallet will not be created if you don't complete the procedure.\n"
         "Are you sure you want to abort the Wallet Creation process?"), parent);
   messageBox.setConfirmButtonText(QObject::tr("Abort Wallet Creation"));
   messageBox.setCancelButtonText(QObject::tr("Back"));

   int result = messageBox.exec();
   return (result == QDialog::Accepted);
}
