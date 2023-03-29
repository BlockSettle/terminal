/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PluginsListModel.h"
#include <QString>

namespace
{
   static const QHash<int, QByteArray> kRoles {
      {PluginsListModel::PluginRoles::Name, "name_role"},
      {PluginsListModel::PluginRoles::Description, "description_role"},
      {PluginsListModel::PluginRoles::Icon, "icon_role"},
      {PluginsListModel::PluginRoles::Path, "path_role"}
   };
}

PluginsListModel::PluginsListModel(QObject* parent)
   : QAbstractListModel(parent)
{}

void PluginsListModel::addPlugins(const std::vector<Plugin*>& plugins)
{
   if (plugins.empty()) {
      return;
   }
   QMetaObject::invokeMethod(this, [this, plugins] {
      beginInsertRows(QModelIndex(), rowCount(), rowCount() + plugins.size() - 1);
      plugins_.insert(plugins_.cend(), plugins.cbegin(), plugins.cend());
      endInsertRows();
      });
}

int PluginsListModel::rowCount(const QModelIndex&) const
{
   return plugins_.size();
}

QVariant PluginsListModel::data(const QModelIndex& index, int role) const
{
   const int row = index.row();
   try {
      switch(role) {
      case Name: return plugins_.at(row)->name();
      case Description: return plugins_.at(row)->description();
      case Icon: return plugins_.at(row)->icon();
      case Path: return plugins_.at(row)->path();
      default: break;
      }
   }
   catch (const std::exception&) {
      return QString{};
   }
   return QVariant();
}

QHash<int, QByteArray> PluginsListModel::roleNames() const
{
   return kRoles;
}

QObject* PluginsListModel::getPlugin(int index)
{
   try {
      return plugins_.at(index);
   }
   catch (...) {
   }
   return nullptr;
}
