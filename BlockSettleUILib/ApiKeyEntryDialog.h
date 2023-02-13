/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef API_KEY_ENTRY_DIALOG_H
#define API_KEY_ENTRY_DIALOG_H

#include <memory>
#include <QDialog>

namespace Ui {
    class ApiKeyEntryDialog;
}

class ApiKeyEntryDialog : public QDialog
{
Q_OBJECT

public:
   explicit ApiKeyEntryDialog(QWidget *parent);
   ~ApiKeyEntryDialog() override;

   static QString getApiKey(QWidget *parent);

private:
   std::unique_ptr<Ui::ApiKeyEntryDialog>   ui_;

};

#endif // API_KEY_ENTRY_DIALOG_H
