/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include <QSortFilterProxyModel>
#include "SettingsController.h"

class PluginsFilterModel: public QSortFilterProxyModel
{
   Q_OBJECT

public:
   PluginsFilterModel(std::shared_ptr<SettingsController> settings);

signals:
   void changed();

protected:
   bool filterAcceptsRow(int source_row,
      const QModelIndex& source_parent) const override;

private:
   std::shared_ptr<SettingsController> settings_;
};
