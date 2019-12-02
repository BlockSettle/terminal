/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RequestPartyBox.h"

RequestPartyBox::RequestPartyBox(const QString& title, const QString& note, QWidget* parent)
   : QDialog(parent), ui_(new Ui::RequestPartyBox)
{
   ui_->setupUi(this);
   ui_->labelTitle->setText(title);
   ui_->labelNote->setText(note);
   resize(width(), 0);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &RequestPartyBox::accept);
   connect(ui_->plainTextEditMessage, &BSChatInput::sendMessage, this, &RequestPartyBox::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &RequestPartyBox::reject);

   setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
}

QString RequestPartyBox::getCustomMessage() const
{
   return ui_->plainTextEditMessage->toPlainText();
}