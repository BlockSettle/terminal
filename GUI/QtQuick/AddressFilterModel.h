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

class AddressFilterModel: public QSortFilterProxyModel
{
   Q_OBJECT
   Q_PROPERTY(bool hideUsed     READ hideUsed     WRITE setHideUsed     NOTIFY changed)
   Q_PROPERTY(bool hideInternal READ hideInternal WRITE setHideInternal NOTIFY changed)
   Q_PROPERTY(bool hideExternal READ hideExternal WRITE setHideExternal NOTIFY changed)
   Q_PROPERTY(bool hideEmpty    READ hideEmpty    WRITE setHideEmpty    NOTIFY changed)

public:
   AddressFilterModel(std::shared_ptr<SettingsController> settings);

   bool hideUsed() const;
   bool hideInternal() const;
   bool hideExternal() const;
   bool hideEmpty() const;
   void setHideUsed(bool hideUsed) noexcept;
   void setHideInternal(bool hideInternal) noexcept;
   void setHideExternal(bool hideExternal) noexcept;
   void setHideEmpty(bool hideEmpty) noexcept;

signals:
   void changed();

protected:
   bool filterAcceptsRow(int source_row,
      const QModelIndex& source_parent) const override;
   bool lessThan(const QModelIndex & left, const QModelIndex & right) const override;

private:
   bool hideUsed_ { true };
   bool hideInternal_ { false };
   bool hideExternal_ { false };
   bool hideEmpty_ { false };
   std::shared_ptr<SettingsController> settings_;
};
