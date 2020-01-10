/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_ACCOUNT_INFO_DIALOG_H__
#define __CELER_ACCOUNT_INFO_DIALOG_H__

#include <QDialog>
#include <memory>

namespace Ui {
    class CelerAccountInfoDialog;
};
class BaseCelerClient;

class CelerAccountInfoDialog : public QDialog
{
Q_OBJECT

public:
   CelerAccountInfoDialog(std::shared_ptr<BaseCelerClient> celerConnection, QWidget* parent = nullptr );
   ~CelerAccountInfoDialog() override;

private:
   std::unique_ptr<Ui::CelerAccountInfoDialog> ui_;
};

#endif // __CELER_ACCOUNT_INFO_DIALOG_H__
