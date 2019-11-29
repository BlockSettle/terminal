/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TerminalEncryptionDialog.h"
#include "ui_TerminalEncryptionDialog.h"
#include <QShowEvent>
#include <QStyle>
#include <QTimer>

// Basic constructor, sets message box type, title and text
TerminalEncryptionDialog::TerminalEncryptionDialog(TerminalEncryptionDialogType dialogType, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::TerminalEncryptionDialog)
{
   ui_->setupUi(this);

   setType(dialogType);
   resize(width(), 0);

   setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &TerminalEncryptionDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &TerminalEncryptionDialog::reject);
}

TerminalEncryptionDialog::~TerminalEncryptionDialog() = default;

void TerminalEncryptionDialog::showEvent( QShowEvent* )
{
   if (parentWidget()) {
      QRect parentRect(parentWidget()->mapToGlobal(QPoint(0, 0)), parentWidget()->size());
      move(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), parentRect).topLeft());
   }
}

void TerminalEncryptionDialog::setOkVisible(bool visible)
{
   ui_->pushButtonOk->setVisible(visible);
}

void TerminalEncryptionDialog::setCancelVisible(bool visible)
{
   ui_->pushButtonCancel->setVisible(visible);
}

void TerminalEncryptionDialog::setPasswordConfirmVisible(bool visible)
{
   ui_->labelConfirmPassword->setVisible(visible);
   ui_->labelConfirmPassword->setMaximumHeight(visible ? SHRT_MAX : 0);

   ui_->lineEditConfirmPassword->setVisible(visible);
   ui_->lineEditConfirmPassword->setMaximumHeight(visible ? SHRT_MAX : 0);
}


void TerminalEncryptionDialog::setType(TerminalEncryptionDialogType type) {
   ui_->labelText->setProperty("h6", true);
   ui_->pushButtonCancel->show();

   ui_->labelText->setMaximumHeight(0);

   switch (type) {
   case Initial:
      ui_->pushButtonCancel->hide();
      ui_->labelText->setMaximumHeight(SHRT_MAX);
      setWindowTitle(tr("Terminal Encryption"));
      break;
   case DisableEncryption:
      setPasswordConfirmVisible(false);
      setWindowTitle(tr("Disable Encryption"));
      break;
   case EnableEncryption:
      setWindowTitle(tr("Enable Encryption"));
      break;
   case Decrypt:
      setPasswordConfirmVisible(false);
      setWindowTitle(tr("Terminal"));
      break;
   }
}

void TerminalEncryptionDialog::setConfirmButtonText(const QString &text) {
   ui_->pushButtonOk->setText(text); 
}

void TerminalEncryptionDialog::setCancelButtonText(const QString &text) {
   ui_->pushButtonCancel->setText(text); 
}
