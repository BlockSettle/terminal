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

   void initSettings() override;

   void display() override;
   void reset() override;
   void apply() override;

   void applyArmoryServers();

signals:
   void reconnectArmory();
   void onArmoryHostChanged();
   void onArmoryPortChanged();

private slots:
   void onEnvSelected(int);

private:
   void DetectEnvironmentSettings();

private:
   std::unique_ptr<Ui::NetworkSettingsPage> ui_;
   ArmoryServersViewModel *armoryServerModel_;
};

#endif // __NETWORK_SETTINGS_PAGE_H__
