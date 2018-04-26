#ifndef __NETWORK_SETTINGS_PAGE_H__
#define __NETWORK_SETTINGS_PAGE_H__

#include <QWidget>

#include <memory>

namespace Ui {
   class NetworkSettingsPage;
};

class ApplicationSettings;

class NetworkSettingsPage : public QWidget
{
Q_OBJECT

public:
   NetworkSettingsPage(QWidget* parent = nullptr);

   void setAppSettings(const std::shared_ptr<ApplicationSettings>& appSettings);

   void displaySettings(bool displayDefault = false);

   void applyChanges();

public slots:
   void onRunArmoryLocallyChecked(bool checked);
   void onNetworkClicked(bool checked);

   void onEnvSettingsChanged();
   void onEnvSelected(int index);

private:
   void DisplayRunArmorySettings(bool runLocally, bool displayDefault);

   void DetectEnvironmentSettings();

private:
   Ui::NetworkSettingsPage *ui_;

   std::shared_ptr<ApplicationSettings> appSettings_;
};

#endif // __NETWORK_SETTINGS_PAGE_H__
