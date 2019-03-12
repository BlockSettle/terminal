#ifndef __GENERAL_SETTINGS_PAGE_H__
#define __GENERAL_SETTINGS_PAGE_H__

#include <memory>
#include "ConfigDialog.h"

namespace Ui {
   class GeneralSettingsPage;
};

class ApplicationSettings;
class WalletsManager;

class GeneralSettingsPage : public SettingsPage
{
public:
   GeneralSettingsPage(QWidget* parent = nullptr);
   ~GeneralSettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;

private slots:
   void onSelectLogFile();
   void onSelectMsgLogFile();
   void onLogFileNameEdited(const QString &txt);
   void onLogLevelChanged(int);

private:
   void checkSettings();

private:
   std::unique_ptr<Ui::GeneralSettingsPage> ui_;
};

#endif // __GENERAL_SETTINGS_PAGE_H__
