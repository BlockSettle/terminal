/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __OPEN_URI_DIALOG_H__
#define __OPEN_URI_DIALOG_H__

#include <memory>
#include <QDialog>
#include <QTimer>

#include "BinaryData.h"
#include "BSErrorCode.h"
#include "EncryptionUtils.h"
#include "ValidityFlag.h"

namespace Ui {
    class OpenURIDialog;
}

class OpenURIDialog : public QDialog
{
Q_OBJECT

public:
   OpenURIDialog(QWidget *parent);
   ~OpenURIDialog() override;

private:
   std::unique_ptr<Ui::OpenURIDialog>   ui_;
};

#endif // __OPEN_URI_DIALOG_H__
