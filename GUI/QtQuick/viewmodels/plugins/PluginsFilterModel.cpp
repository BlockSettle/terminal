/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PluginsFilterModel.h"
#include "PluginsListModel.h"

PluginsFilterModel::PluginsFilterModel(std::shared_ptr<SettingsController> settings)
   : QSortFilterProxyModel()
   , settings_(settings)
{
   connect(this, &PluginsFilterModel::changed, this, &PluginsFilterModel::invalidate);
}

bool PluginsFilterModel::filterAcceptsRow(int source_row,
   const QModelIndex& source_parent) const
{
   return true;
}
