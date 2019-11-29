/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __COMMON_MESSAGE_BOX_DIALOG_H__
#define __COMMON_MESSAGE_BOX_DIALOG_H__

#include <QDialog>

class CommonMessageBoxDialog : public QDialog
{
Q_OBJECT
public:
   CommonMessageBoxDialog(QWidget* parent = nullptr);
   ~CommonMessageBoxDialog() noexcept override = default;

protected:
   void showEvent(QShowEvent *event) override;
   void UpdateSize();
};

#endif // __COMMON_MESSAGE_BOX_DIALOG_H__