/*

***********************************************************************************
* Copyright (C) 2020 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef API_SETTINGS_PAGE_H
#define API_SETTINGS_PAGE_H

#include <memory>
#include "ConfigDialog.h"

namespace Ui {
   class APISettingsPage;
}
class AutoRFQ;

class APISettingsPage : public SettingsPage
{
public:
   APISettingsPage(QWidget* parent = nullptr);
   ~APISettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;

private:
   std::unique_ptr<Ui::APISettingsPage> ui_;
};

#endif // API_SETTINGS_PAGE_H
