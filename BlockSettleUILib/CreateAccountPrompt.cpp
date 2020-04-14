/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CreateAccountPrompt.h"

#include "ui_CreateAccountPrompt.h"

CreateAccountPrompt::CreateAccountPrompt(QWidget *parent) :
   QDialog(parent),
   ui_(new Ui::CreateAccountPrompt)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCreateAccount, &QPushButton::clicked, this, [this] {
      done(CreateAccount);
   });

   connect(ui_->pushButtonLogin, &QPushButton::clicked, this, [this] {
      done(Login);
   });

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, [this] {
      done(Cancel);
   });
}

CreateAccountPrompt::~CreateAccountPrompt() = default;
