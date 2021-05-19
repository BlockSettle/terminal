/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   Q_OBJECT
public:
   GeneralSettingsPage(QWidget* parent = nullptr);
   ~GeneralSettingsPage() override;

   void init(const ApplicationSettings::State&) override;
   void display() override;
   void reset() override;
   void apply() override;

private slots:
   void onSelectLogFile();
   void onSelectMsgLogFile();
   void onLogFileNameEdited(const QString &txt);
   void onLogLevelChanged(int);

private:
   void checkLogSettings();

signals:
   void requestDataEncryption();

private:
   std::unique_ptr<Ui::GeneralSettingsPage> ui_;
};

#endif // __GENERAL_SETTINGS_PAGE_H__
