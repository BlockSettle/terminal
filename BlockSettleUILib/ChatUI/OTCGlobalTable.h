/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __OTC_GLOBAL_TABLE_H__
#define __OTC_GLOBAL_TABLE_H__

#include "ChatUI/ChatUsersViewItemStyle.h"
#include "TreeViewWithEnterKey.h"
#include "ProgressViewDelegateBase.h"

class OTCGlobalTable : public TreeViewWithEnterKey
{
    Q_OBJECT
public:
    explicit OTCGlobalTable(QWidget* parent = nullptr);
    ~OTCGlobalTable() override = default;

protected:
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option,
       const QModelIndex& index) const override;

private:
   ChatUsersViewItemStyle itemStyle_;
};

class OTCRequestsProgressDelegate : public ProgressViewDelegateBase
{
public:
   explicit OTCRequestsProgressDelegate(QWidget* parent = nullptr)
      : ProgressViewDelegateBase(parent)
   {}
   ~OTCRequestsProgressDelegate() override = default;
protected:
   bool isDrawProgressBar(const QModelIndex& index) const override;
   int maxValue(const QModelIndex& index) const override;
   int currentValue(const QModelIndex& index) const override;
};

class LeftOffsetDelegate : public QStyledItemDelegate
{
public:
   explicit LeftOffsetDelegate(QWidget* parent = nullptr)
      : QStyledItemDelegate(parent)
   {}
   ~LeftOffsetDelegate() override = default;

   void paint(QPainter* painter, const QStyleOptionViewItem& opt,
      const QModelIndex& index) const override;
};

#endif // __OTC_GLOBAL_TABLE_H__
