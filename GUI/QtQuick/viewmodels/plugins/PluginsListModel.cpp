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
      {PluginsListModel::PluginRoles::Name, "name"},
      {PluginsListModel::PluginRoles::Description, "description"},
      {PluginsListModel::PluginRoles::Icon, "icon"},
      {PluginsListModel::PluginRoles::IsInstalled, "isInstalled"}
   };
}

PluginsListModel::PluginsListModel(QObject* parent)
   : QAbstractListModel(parent)
{
   // temporary model
   plugins_ = {
      { tr("SideShift.ai v1")
      , tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies")
      , QString::fromLatin1("qrc:/images/sideshift_plugin.png")
      , false },
      { tr("SideShift.ai v2")
      , tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies")
      , QString::fromLatin1("qrc:/images/sideshift_plugin.png")
      , false },
      { tr("SideShift.ai v3")
      , tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies")
      , QString::fromLatin1("qrc:/images/sideshift_plugin.png")
      , false },
      { tr("SideShift.ai v4")
      , tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies")
      , QString::fromLatin1("qrc:/images/sideshift_plugin.png")
      , false },
      { tr("SideShift.ai v5")
      , tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies")
      , QString::fromLatin1("qrc:/images/sideshift_plugin.png")
      , false },
      { tr("SideShift.ai v6")
      , tr("Shift between BTC, ETH, BCH, XMR, USDT and 90+ other cryptocurrencies")
      , QString::fromLatin1("qrc:/images/sideshift_plugin.png")
      , false }
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
      case IsInstalled:  return plugins_.at(row).isInstalled;
      default: break;
      }
   }
   catch (const std::exception&) {
      return QString{};
   }
   return QVariant();
}

bool PluginsListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
   if(role == IsInstalled) {
      plugins_.at(index.row()).isInstalled = value.toBool();
      emit dataChanged(index, index);
      return true;
   }
   return false;
}

QHash<int, QByteArray> PluginsListModel::roleNames() const
{
   return kRoles;
}
