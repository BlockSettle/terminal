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
      {PluginsListModel::PluginRoles::Icon, "icon_role"}
   };
}

PluginsListModel::PluginsListModel(QObject* parent)
   : QAbstractListModel(parent)
{
   plugins_ = {
      { tr("SideShift.ai")
      , tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies")
      , QString::fromLatin1("qrc:/images/sideshift_plugin.png") }
   };
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
      case Name: return plugins_.at(row).name;
      case Description: return plugins_.at(row).description;
      case Icon: return plugins_.at(row).icon;
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
