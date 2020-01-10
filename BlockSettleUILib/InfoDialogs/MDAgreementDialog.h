/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __MD_AGREEMENT_DIALOG_H__
#define __MD_AGREEMENT_DIALOG_H__

#include <QDialog>

namespace Ui {
    class MDAgreementDialog;
};

class MDAgreementDialog : public QDialog
{
Q_OBJECT

public:
   MDAgreementDialog(QWidget* parent = nullptr );
   ~MDAgreementDialog() override = default;

private slots:
   void OnContinuePressed();

private:
   Ui::MDAgreementDialog* ui_;
};

#endif // __MD_AGREEMENT_DIALOG_H__