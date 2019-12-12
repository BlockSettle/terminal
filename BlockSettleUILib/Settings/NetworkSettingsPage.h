/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __NETWORK_SETTINGS_PAGE_H__
#define __NETWORK_SETTINGS_PAGE_H__

#include <memory>
#include "ConfigDialog.h"
#include "ArmoryServersViewModel.h"

namespace Ui {
   class NetworkSettingsPage;
}

class ApplicationSettings;

class NetworkSettingsPage : public SettingsPage
{
   Q_OBJECT

public:
   NetworkSettingsPage(QWidget* parent = nullptr);
   ~NetworkSettingsPage() override;

public slots:
   void initSettings() override;
   void display() override;
   void reset() override;
   void apply() override;

signals:
   void reconnectArmory();
   void armoryServerChanged();

private slots:
   void onEnvSelected(int);
   void displayArmorySettings();
   void displayEnvironmentSettings();

private:
   std::unique_ptr<Ui::NetworkSettingsPage> ui_;
   ArmoryServersViewModel *armoryServerModel_;
   bool disableSettingUpdate_{false};
};

#endif // __NETWORK_SETTINGS_PAGE_H__
