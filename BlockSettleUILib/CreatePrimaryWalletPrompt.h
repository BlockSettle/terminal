/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CREATE_PRIMARY_WALLET_PROMPT_H
#define CREATE_PRIMARY_WALLET_PROMPT_H

#include <memory>
#include <QDialog>

namespace Ui {
   class CreatePrimaryWalletPrompt;
}

class CreatePrimaryWalletPrompt : public QDialog
{
   Q_OBJECT

public:
   enum Result
   {
      Cancel = QDialog::Rejected,
      CreateWallet = QDialog::Accepted,
      ImportWallet,
   };

   explicit CreatePrimaryWalletPrompt(QWidget *parent = nullptr);
   ~CreatePrimaryWalletPrompt();

private:
   std::unique_ptr<Ui::CreatePrimaryWalletPrompt> ui_;

};

#endif
