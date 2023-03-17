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

struct Plugin {
   QString name;
   QString description;
   QString icon;
   bool isInstalled;
};

class PluginsListModel: public QAbstractListModel
{
   Q_OBJECT
public:
   enum PluginRoles
   {
      Name = Qt::UserRole + 1,
      Description,
      Icon,
      IsInstalled
   };
   Q_ENUM(PluginRoles)

   PluginsListModel(QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;
   bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private:
   std::vector<Plugin> plugins_;
};

