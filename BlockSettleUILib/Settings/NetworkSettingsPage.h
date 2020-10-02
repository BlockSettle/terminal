/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
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
class ArmoryServersWidget;

class NetworkSettingsPage : public SettingsPage
{
   Q_OBJECT
public:
   NetworkSettingsPage(QWidget* parent = nullptr);
   ~NetworkSettingsPage() override;

   void init(const ApplicationSettings::State&) override;

   void onArmoryServers(const QList<ArmoryServer>&, int idxCur, int idxConn);

public slots:
   void initSettings() override;
   void display() override;
   void reset() override;
   void apply() override;

signals:
   void reconnectArmory();
   void armoryServerChanged();
   void setArmoryServer(int);
   void addArmoryServer(const ArmoryServer&);
   void delArmoryServer(int);
   void updArmoryServer(int, const ArmoryServer&);

private slots:
   void onEnvSelected(int index);
   void onArmorySelected(int armoryIndex);
   void displayArmorySettings();
   void displayEnvironmentSettings();

private:
   void applyLocalSignerNetOption();

private:
   std::unique_ptr<Ui::NetworkSettingsPage> ui_;
   ArmoryServersViewModel* armoryServerModel_{ nullptr };
   ArmoryServersWidget* armoryServersWidget_{ nullptr };
   bool disableSettingUpdate_{true};
   QList<ArmoryServer>  armoryServers_;
   int   armorySrvCurrent_{ 0 };
   int   armorySrvConnected_{ 0 };
};

#endif // __NETWORK_SETTINGS_PAGE_H__
