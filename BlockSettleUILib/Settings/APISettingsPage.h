/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef API_SETTINGS_PAGE_H
#define API_SETTINGS_PAGE_H

#include <memory>
#include "ConfigDialog.h"
#include "ValidityFlag.h"

namespace Ui {
   class APISettingsPage;
}
class AutoRFQ;

//TODO: rework to allow incoming API connections
class APISettingsPage : public SettingsPage
{
public:
   APISettingsPage(QWidget* parent = nullptr);
   ~APISettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;

private slots:
   void onApiKeyImport();

private:
   void updateApiKeyStatus();

   std::unique_ptr<Ui::APISettingsPage> ui_;
   std::string apiKeyEncrypted_;
   ValidityFlag validityFlag_;
};

#endif // API_SETTINGS_PAGE_H
