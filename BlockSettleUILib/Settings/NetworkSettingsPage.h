/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __NETWORK_SETTINGS_PAGE_H__
#define __NETWORK_SETTINGS_PAGE_H__

#include <memory>
#include "ConfigDialog.h"

namespace Ui {
   class NetworkSettingsPage;
}

class ApplicationSettings;
class ArmoryServersViewModel;

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
   void onEnvSelected(int index);
   void onArmorySelected(int armoryIndex);
   void displayArmorySettings();
   void displayEnvironmentSettings();

private:
   void applyLocalSignerNetOption();

private:
   std::unique_ptr<Ui::NetworkSettingsPage> ui_;
   ArmoryServersViewModel *armoryServerModel_;
   bool disableSettingUpdate_{true};
};

#endif // __NETWORK_SETTINGS_PAGE_H__
