/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AddressFilterModel.h"
#include "AddressListModel.h"
#include <QtGlobal>

AddressFilterModel::AddressFilterModel(std::shared_ptr<SettingsController> settings)
   : QSortFilterProxyModel()
   , settings_(settings)
{
   connect(this, &AddressFilterModel::changed, this, &AddressFilterModel::invalidate);

   if (settings_ != nullptr)
   {
      connect(settings_.get(), &SettingsController::reset, this, [this]()
      {
         if (settings_->hasParam(ApplicationSettings::Setting::AddressFilterHideUsed)) {
            hideUsed_ = settings_->getParam(ApplicationSettings::Setting::AddressFilterHideUsed).toBool();
         }
         if (settings_->hasParam(ApplicationSettings::Setting::AddressFilterHideInternal)) {
            hideInternal_ = settings_->getParam(ApplicationSettings::Setting::AddressFilterHideInternal).toBool();
         }
         if (settings_->hasParam(ApplicationSettings::Setting::AddressFilterHideExternal)) {
            hideExternal_ = settings_->getParam(ApplicationSettings::Setting::AddressFilterHideExternal).toBool();
         }
         if (settings_->hasParam(ApplicationSettings::Setting::AddressFilterHideEmpty)) {
            hideEmpty_ = settings_->getParam(ApplicationSettings::Setting::AddressFilterHideEmpty).toBool();
         }
         emit changed();
      });
   }
}

bool AddressFilterModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
   if (hideUsed_) {
      const auto txCountIndex = sourceModel()->index(source_row, 1);
      const auto bananceIndex = sourceModel()->index(source_row, 2);
      if (sourceModel()->data(txCountIndex, QmlAddressListModel::TableRoles::TableDataRole) != 0 &&
          qFuzzyIsNull(sourceModel()->data(bananceIndex, QmlAddressListModel::TableRoles::TableDataRole).toDouble()))
      {
         return false;
      }
   }

   if (hideEmpty_) {
      const auto bananceIndex = sourceModel()->index(source_row, 2);
      if (qFuzzyIsNull(sourceModel()->data(bananceIndex, QmlAddressListModel::TableRoles::TableDataRole).toDouble()))
      {
         return false;
      }
   }

   const auto assetTypeIndex = sourceModel()->index(source_row, 0);
   const auto assetType = sourceModel()->data(assetTypeIndex, QmlAddressListModel::TableRoles::AddressTypeRole).toString();
   if (hideInternal_) {
      if (assetType.length() > 0 && assetType.at(0).toLatin1() == '1')
      {
         return false;
      }
   }
   if (hideExternal_) {
      if (assetType.length() > 0 && assetType.at(0).toLatin1() == '0')
      {
         return false;
      }
   }

   return true;
}

bool AddressFilterModel::hideUsed() const
{
   return hideUsed_;
}

bool AddressFilterModel::hideInternal() const
{
   return hideInternal_;
}

bool AddressFilterModel::hideExternal() const
{
   return hideExternal_;
}

bool AddressFilterModel::hideEmpty() const
{
   return hideEmpty_;
}

void AddressFilterModel::setHideUsed(bool hideUsed) noexcept
{
   if (hideUsed != hideUsed_) {
      hideUsed_ = hideUsed;
      settings_->setParam(ApplicationSettings::Setting::AddressFilterHideUsed, hideUsed_);
      emit changed();
   }
}

void AddressFilterModel::setHideInternal(bool hideInternal) noexcept
{
   if (hideInternal_ != hideInternal) {
      hideInternal_ = hideInternal;
      settings_->setParam(ApplicationSettings::Setting::AddressFilterHideInternal, hideInternal_);
      emit changed();
   }
}

void AddressFilterModel::setHideExternal(bool hideExternal) noexcept
{
   if (hideExternal_ != hideExternal) {
      hideExternal_ = hideExternal;
      settings_->setParam(ApplicationSettings::Setting::AddressFilterHideExternal, hideExternal_);
      emit changed();
   }
}

void AddressFilterModel::setHideEmpty(bool hideEmpty) noexcept
{
   if (hideEmpty_ != hideEmpty) {
      hideEmpty_ = hideEmpty;
      settings_->setParam(ApplicationSettings::Setting::AddressFilterHideEmpty, hideEmpty_);
      emit changed();
   }
}
