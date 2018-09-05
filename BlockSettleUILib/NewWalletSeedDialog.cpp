#include "NewWalletSeedDialog.h"

#include <QDir>
#include <QFileDialog>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QStandardPaths>
#include <QValidator>
#include <QResizeEvent>

#include "MessageBoxCritical.h"
#include "MessageBoxQuestion.h"
#include "PaperBackupWriter.h"
#include "UiUtils.h"
#include "ui_NewWalletSeedDialog.h"

namespace {

   const double kMarginScale = 0.5;

   const int kTotalClientPadding = 50;

}

//
// SeedValidator
//

//! Simplest validator for key that just checks count of characters.
class SeedValidator : public QValidator
{
public:
   explicit SeedValidator(QObject *parent)
      : QValidator(parent)
   {
   }

   QValidator::State validate(QString &input, int &pos) const override
   {
      QString key = input.trimmed();
      key.remove(QChar::Space);

      if (key.length() > maxLength_) {
         cutInput(input, pos);

         return QValidator::Invalid;
      } else {
         splitInput(key, input, pos);

         return QValidator::Acceptable;
      }
   }

private:
   void cutInput(QString &input, int &pos) const
   {
      int i = 0;

      for (const auto &ch : qAsConst(input)) {
         if (ch != QChar::Space) {
            ++i;
         }

         if (i == maxLength_) {
            pos = i;
            input.remove(i, input.length() - i);
            return;
         }
      }
   }

   void splitInput(const QString &key, QString &input, int &pos) const
   {
      QString splitted;
      int i = 0;

      for (const auto &ch : qAsConst(key)) {
         if (i == 4) {
            splitted.append(QChar::Space);
            i = 0;
         }

         ++i;

         splitted.append(ch);
      }

      if (input.length() == pos) {
         pos = splitted.length();
      }

      input = splitted;
   }

private:
   static const int maxLength_ = 9 * 4;
}; // class SeedValidator


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

   const auto pdfPreview = pdfWriter_->getPreview(width() - kTotalClientPadding, kMarginScale);

   ui_->labelPreview->setPixmap(pdfPreview);

   connect(ui_->pushButtonSave, &QPushButton::clicked, this, &NewWalletSeedDialog::onSaveClicked);
   connect(ui_->pushButtonPrint, &QPushButton::clicked, this, &NewWalletSeedDialog::onPrintClicked);
   connect(ui_->pushButtonContinue, &QPushButton::clicked, this, &NewWalletSeedDialog::onContinueClicked);
   connect(ui_->pushButtonBack, &QPushButton::clicked, this, &NewWalletSeedDialog::onBackClicked);
   connect(ui_->lineEditLine1, &QLineEdit::textChanged, this, &NewWalletSeedDialog::onKeyChanged);
   connect(ui_->lineEditLine2, &QLineEdit::textChanged, this, &NewWalletSeedDialog::onKeyChanged);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &NewWalletSeedDialog::reject);

   auto * validator = new SeedValidator(this);
   ui_->lineEditLine1->setValidator(validator);
   ui_->lineEditLine2->setValidator(validator);

   setCurrentPage(Pages::PrintPreview);
   updateState();
}

NewWalletSeedDialog::~NewWalletSeedDialog() = default;

void NewWalletSeedDialog::resizeEvent(QResizeEvent *e)
{
   const auto pdfPreview = pdfWriter_->getPreview(width() - kTotalClientPadding, kMarginScale);

   ui_->labelPreview->setPixmap(pdfPreview);

   e->accept();
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
      MessageBoxCritical messageBox(tr("Failed to save backup file")
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
      MessageBoxCritical messageBox(tr("Printing Error")
         , tr("Please make sure that you have printer connected."), this);
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
      MessageBoxCritical messageBox(tr("Check failed!")
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
   MessageBoxQuestion messageBox(QObject::tr("Warning"), QObject::tr("ABORT WALLET CREATION?")
      , QObject::tr("The Wallet will not be created if you don't complete the procedure.\n"
         "Are you sure you want to abort the Wallet Creation process?"), parent);
   messageBox.setConfirmButtonText(QObject::tr("Abort Wallet Creation")).setCancelButtonText(QObject::tr("Back"));

   int result = messageBox.exec();
   return (result == QDialog::Accepted);
}
