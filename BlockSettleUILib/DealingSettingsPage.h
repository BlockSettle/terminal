#ifndef DEALING_SETTINGS_PAGE_H
#define DEALING_SETTINGS_PAGE_H

#include <QWidget>

#include <memory>

namespace Ui {
   class DealingSettingsPage;
};

class ApplicationSettings;
class AssetManager;
class SecuritiesModel;

class DealingSettingsPage : public QWidget
{
Q_OBJECT

public:
   DealingSettingsPage(QWidget* parent = nullptr);

   void setAppSettings(const std::shared_ptr<ApplicationSettings>& appSettings);

   void displaySettings(const std::shared_ptr<AssetManager> &assetMgr
      , bool displayDefault = false);

   void applyChanges();

protected slots:
   void onResetCountes();

private:
   Ui::DealingSettingsPage *ui_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
};

#endif // DEALING_SETTINGS_PAGE_H
