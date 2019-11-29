/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TERMINAL_ENCRYPTION_DIALOG_H__
#define __TERMINAL_ENCRYPTION_DIALOG_H__

#include <QDialog>
#include <memory>

namespace Ui
{
   class TerminalEncryptionDialog;
}

class TerminalEncryptionDialog : public QDialog
{
Q_OBJECT

public:
   enum TerminalEncryptionDialogType {
      Initial = 1,
      DisableEncryption = 2,
      EnableEncryption = 3,
      Decrypt = 4
   };
   Q_ENUM(TerminalEncryptionDialogType)

   TerminalEncryptionDialog(TerminalEncryptionDialogType dialogType
      , QWidget* parent = nullptr);

   ~TerminalEncryptionDialog() override;
   void setConfirmButtonText(const QString &text);
   void setCancelButtonText(const QString &text);

   void showEvent(QShowEvent *) override;

   void setOkVisible(bool visible);
   void setCancelVisible(bool visible);
   void setPasswordConfirmVisible(bool visible);


private:
   void setType(TerminalEncryptionDialogType type);

private:
   std::unique_ptr<Ui::TerminalEncryptionDialog> ui_;
};


#endif // __TERMINAL_ENCRYPTION_DIALOG_H__
