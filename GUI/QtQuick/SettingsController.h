/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include <QObject>
#include <QVariant>
#include "ApplicationSettings.h"

class SettingsController: public QObject
{
   Q_OBJECT
   using SettingsCache = std::map<ApplicationSettings::Setting, QVariant>;

public:
   SettingsController();
   SettingsController(const SettingsController& settingsController);
   SettingsController& operator=(const SettingsController& settingsController);

   bool hasParam(ApplicationSettings::Setting key) const;
   const QVariant& getParam(ApplicationSettings::Setting key) const;
   void setParam(ApplicationSettings::Setting key, const QVariant& value);

   const SettingsCache& getCache() const;
   void resetCache(const SettingsCache& cache);

signals:
   void changed(ApplicationSettings::Setting);
   void reset();

private:
   SettingsCache settingsCache_;
};
