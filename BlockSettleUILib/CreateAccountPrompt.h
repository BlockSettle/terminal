/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CREATE_ACCOUNT_PROMPT_H
#define CREATE_ACCOUNT_PROMPT_H

#include <memory>
#include <QDialog>


namespace Ui {
   class CreateAccountPrompt;
}

class CreateAccountPrompt : public QDialog
{
   Q_OBJECT
public:
   enum Result
   {
      Cancel = QDialog::Rejected,
      CreateAccount = QDialog::Accepted,
      Login,
   };

   explicit CreateAccountPrompt(QWidget *parent = nullptr);
   ~CreateAccountPrompt() override;

private:
   std::unique_ptr<Ui::CreateAccountPrompt> ui_;

};

#endif
