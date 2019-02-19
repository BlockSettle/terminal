#ifndef DEALING_SETTINGS_PAGE_H
#define DEALING_SETTINGS_PAGE_H

#include <memory>
#include "ConfigDialog.h"

namespace Ui {
   class DealingSettingsPage;
};

class ApplicationSettings;
class AssetManager;
class SecuritiesModel;

class DealingSettingsPage : public SettingsPage
{
public:
   DealingSettingsPage(QWidget* parent = nullptr);
   ~DealingSettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;

private slots:
   void onResetCountes();

private:
   std::unique_ptr<Ui::DealingSettingsPage> ui_;
};

#endif // DEALING_SETTINGS_PAGE_H
