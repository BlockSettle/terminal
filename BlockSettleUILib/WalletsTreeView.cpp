#include "WalletsTreeView.h"

WalletsTreeView::WalletsTreeView(QWidget *parent)
    : TreeViewWithEnterKey(parent)
{
    setAlternatingRowColors(true);
    setItemsExpandable(true);
    setRootIsDecorated(false);
}

void WalletsTreeView::resizeColumns()
{
    if (model() != nullptr) {
        for (int i = model()->columnCount() - 1; i >= 0; --i) {
            resizeColumnToContents(i);
        }
    }
}
