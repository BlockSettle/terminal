#ifndef __NETWORK_SETTINGS_PAGE_H__
#define __NETWORK_SETTINGS_PAGE_H__

#include <memory>
#include "ConfigDialog.h"

namespace Ui {
   class NetworkSettingsPage;
};

class ApplicationSettings;

class NetworkSettingsPage : public SettingsPage
{
public:
   NetworkSettingsPage(QWidget* parent = nullptr);
   ~NetworkSettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;

private slots:
   void onRunArmoryLocallyChecked(bool checked);
   void onNetworkClicked(bool checked);

   void onArmoryHostChanged();
   void onArmoryPortChanged();

private:
   void DisplayRunArmorySettings(bool runLocally);

private:
   std::unique_ptr<Ui::NetworkSettingsPage> ui_;
};

#endif // __NETWORK_SETTINGS_PAGE_H__
