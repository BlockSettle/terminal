/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
