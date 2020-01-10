/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CHANGE_LOG_DIALOG_H__
#define __CHANGE_LOG_DIALOG_H__

#include <QDialog>
#include "VersionChecker.h"

#include <memory>

namespace Ui {
    class ChangeLogDialog;
};

class ChangeLogDialog : public QDialog
{
Q_OBJECT

public:
   ChangeLogDialog(const bs::VersionChecker& verChecker, QWidget* parent = nullptr );
   ~ChangeLogDialog() override;

private:
   std::unique_ptr<Ui::ChangeLogDialog> ui_;
};

#endif // __CHANGE_LOG_DIALOG_H__
