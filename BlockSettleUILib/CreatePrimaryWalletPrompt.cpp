/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CreatePrimaryWalletPrompt.h"

#include "ui_CreatePrimaryWalletPrompt.h"

CreatePrimaryWalletPrompt::CreatePrimaryWalletPrompt(QWidget *parent) :
   QDialog(parent),
   ui_(new Ui::CreatePrimaryWalletPrompt)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, [this] {
      done(Cancel);
   });
   connect(ui_->pushButtonCreate, &QPushButton::clicked, this, [this] {
      done(CreateWallet);
   });
   connect(ui_->pushButtonImport, &QPushButton::clicked, this, [this] {
      done(ImportWallet);
   });
}

CreatePrimaryWalletPrompt::~CreatePrimaryWalletPrompt() = default;
