#ifndef __OTC_GLOBAL_TABLE_H__
#define __OTC_GLOBAL_TABLE_H__

#include "TreeViewWithEnterKey.h"
#include "ChatUI/ChatUsersViewItemStyle.h"

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

#endif // __OTC_GLOBAL_TABLE_H__
