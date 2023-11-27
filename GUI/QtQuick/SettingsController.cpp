/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SettingsController.h"

SettingsController::SettingsController()
   : QObject()
{
}

SettingsController::SettingsController(const SettingsController& settingsController)
   : settingsCache_(settingsController.settingsCache_)
{
}

SettingsController& SettingsController::operator=(const SettingsController& other)
{
   settingsCache_ = other.settingsCache_;
   return *this;
}

void SettingsController::resetCache(const SettingsController::SettingsCache& cache)
{
   settingsCache_ = cache;
   emit reset();
}

const SettingsController::SettingsCache& SettingsController::getCache() const
{
   return settingsCache_;
}

void SettingsController::setParam(ApplicationSettings::Setting key, const QVariant& value)
{
   if (settingsCache_.count(key) == 0 || settingsCache_.at(key) != value) {
      settingsCache_[key] = value;
      emit changed(key);
   }
}

const QVariant& SettingsController::getParam(ApplicationSettings::Setting key) const
{
   if (settingsCache_.count(key) > 0) {
      return settingsCache_.at(key);
   }
   return QVariant();
}

bool SettingsController::hasParam(ApplicationSettings::Setting key) const
{
   return settingsCache_.count(key) > 0;
}
