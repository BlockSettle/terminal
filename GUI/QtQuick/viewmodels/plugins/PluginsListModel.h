/*

***********************************************************************************
* Copyright (C) 2023 BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QVariant>
#include <QColor>
#include <memory>
#include "Plugin.h"


class PluginsListModel: public QAbstractListModel
{
   Q_OBJECT
public:
   enum PluginRoles
   {
      Name = Qt::UserRole + 1,
      Description,
      Icon,
      Path
   };
   Q_ENUM(PluginRoles)

   PluginsListModel(QObject* parent = nullptr);
   void addPlugins(const std::vector<Plugin*>&);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;

   Q_INVOKABLE QObject* getPlugin(int index);

private:
   std::vector<Plugin*> plugins_;
};
